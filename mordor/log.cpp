// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "log.h"

#include <iostream>

#include <boost/bind.hpp>
#include <boost/regex.hpp>

#include "assert.h"
#include "config.h"
#include "fiber.h"
#include "mordor/streams/file.h"

namespace Mordor {

static void enableLoggers();
static void enableStdoutLogging();
static void enableFileLogging();

static ConfigVar<std::string>::ptr g_logFatal =
    Config::lookup("log.fatalmask", std::string(".*"), "Regex of loggers to enable fatal for.");
static ConfigVar<std::string>::ptr g_logError =
    Config::lookup("log.errormask", std::string(".*"), "Regex of loggers to enable error for.");
static ConfigVar<std::string>::ptr g_logWarn =
    Config::lookup("log.warnmask", std::string(".*"), "Regex of loggers to enable warning for.");
static ConfigVar<std::string>::ptr g_logInfo =
    Config::lookup("log.infomask", std::string(".*"), "Regex of loggers to enable info for.");
static ConfigVar<std::string>::ptr g_logTrace =
    Config::lookup("log.tracemask", std::string(""), "Regex of loggers to enable trace for.");
static ConfigVar<std::string>::ptr g_logVerbose =
    Config::lookup("log.verbosemask", std::string(""), "Regex of loggers to enable verbose for.");

static ConfigVar<bool>::ptr g_logStdout =
    Config::lookup("log.stdout", false, "Log to stdout");
static ConfigVar<std::string>::ptr g_logFile =
    Config::lookup("log.file", std::string(""), "Log to file");

namespace {

static struct Initializer
{
    Initializer()
    {
        g_logFatal->monitor(&enableLoggers);
        g_logError->monitor(&enableLoggers);
        g_logWarn->monitor(&enableLoggers);
        g_logInfo->monitor(&enableLoggers);
        g_logTrace->monitor(&enableLoggers);
        g_logVerbose->monitor(&enableLoggers);

        g_logFile->monitor(&enableFileLogging);
        g_logStdout->monitor(&enableStdoutLogging);
    }
} g_init;

}

static void enableLogger(Logger::ptr logger, const boost::regex &fatalRegex,
    const boost::regex &errorRegex, const boost::regex &warnRegex,
    const boost::regex &infoRegex, const boost::regex &traceRegex,
    const boost::regex &verboseRegex)
{
    Log::Level level = Log::NONE;
    if (boost::regex_match(logger->name(), fatalRegex))
        level = Log::FATAL;
    if (boost::regex_match(logger->name(), errorRegex))
        level = Log::ERROR;
    if (boost::regex_match(logger->name(), warnRegex))
        level = Log::WARNING;
    if (boost::regex_match(logger->name(), infoRegex))
        level = Log::INFO;
    if (boost::regex_match(logger->name(), traceRegex))
        level = Log::TRACE;
    if (boost::regex_match(logger->name(), verboseRegex))
        level = Log::VERBOSE;

    if (logger->level() != level)
        logger->level(level, false);
}

static void enableLoggers()
{
    boost::regex fatalRegex("^" + g_logFatal->val() + "$");
    boost::regex errorRegex("^" + g_logError->val() + "$");
    boost::regex warnRegex("^" + g_logWarn->val() + "$");
    boost::regex infoRegex("^" + g_logInfo->val() + "$");
    boost::regex traceRegex("^" + g_logTrace->val() + "$");
    boost::regex verboseRegex("^" + g_logVerbose->val() + "$");
    Log::visit(boost::bind(&enableLogger, _1, boost::cref(fatalRegex),
        boost::cref(errorRegex), boost::cref(warnRegex),
        boost::cref(infoRegex), boost::cref(traceRegex),
        boost::cref(verboseRegex)));
}

static void enableStdoutLogging()
{
    static LogSink::ptr stdoutSink;
    bool log = g_logStdout->val();
    if (stdoutSink.get() && !log) {
        Log::removeSink(stdoutSink);
        stdoutSink.reset();
    } else if (!stdoutSink.get() && log) {
        stdoutSink.reset(new StdoutLogSink());
        Log::addSink(stdoutSink);
    }
}

static void enableFileLogging()
{
    static LogSink::ptr fileSink;
    std::string file = g_logFile->val();
    if (fileSink.get() && file.empty()) {
        Log::removeSink(fileSink);
        fileSink.reset();
    } else if (!file.empty()) {
        if (fileSink.get()) {
            if (static_cast<FileLogSink*>(fileSink.get())->file() == file)
                return;
            Log::removeSink(fileSink);
            fileSink.reset();
        }
        fileSink.reset(new FileLogSink(file));
        Log::addSink(fileSink);
    }
}

void
StdoutLogSink::log(const std::string &logger, tid_t thread,
    void *fiber, Log::Level level,
    const std::string &str, const char *file, int line)
{
    std::ostringstream os;
    if (file) {
        os << level << " " << thread << " " << fiber << " "
            << logger << " " << file << ":" << line << " " << str << std::endl;
    } else {
        os << level << " " << thread << " " << fiber << " "
            << logger << " " << str << std::endl;
    }
    std::cout << os.str();
    std::cout.flush();
}

FileLogSink::FileLogSink(const std::string &file)
{
    m_stream.reset(new FileStream(file, FileStream::APPEND));
    m_file = file;
}

void
FileLogSink::log(const std::string &logger, tid_t thread,
    void *fiber, Log::Level level,
    const std::string &str, const char *file, int line)
{
    std::ostringstream os;
    if (file) {
        os << level << " " << thread << " " << fiber << " "
            << logger << " " << file << ":" << line << " " << str << std::endl;
    } else {
        os << level << " " << thread << " " << fiber << " "
            << logger << " " << str << std::endl;
    }
    std::string logline = os.str();
    m_stream->write(logline.c_str(), logline.size());
    m_stream->flush();
}

static void deleteNothing(Logger *l)
{}

Logger::ptr Log::root()
{
    static Logger::ptr _root(new Logger());
    return _root;
}

Logger::ptr Log::lookup(const std::string &name)
{
    Logger::ptr log = root();
    std::set<Logger::ptr, LoggerLess>::iterator it;
    Logger dummy(name, log);
    Logger::ptr dummyPtr(&dummy, &deleteNothing);
    while (true) {
        it = log->m_children.lower_bound(dummyPtr);
        if (it == log->m_children.end()) {
            Logger::ptr child(new Logger(name, log));
            log->m_children.insert(child);
            return child;
        }
        if ((*it)->m_name == name)
            return *it;
        // Child of existing logger
        if (name.length() > (*it)->m_name.length() &&
            name[(*it)->m_name.length()] == ':' &&
            strncmp((*it)->m_name.c_str(), name.c_str(), (*it)->m_name.length()) == 0) {
            log = *it;
            continue;
        }
        ++it;
        // Existing logger should be child of this logger
        if (it != log->m_children.end() &&
            (*it)->m_name.length() > name.length() &&
            (*it)->m_name[name.length()] == ':' &&
            strncmp((*it)->m_name.c_str(), name.c_str(), name.length()) == 0) {
            Logger::ptr child = *it;
            Logger::ptr parent(new Logger(name, log));
            log->m_children.erase(it);
            log->m_children.insert(parent);
            parent->m_children.insert(child);
            child->m_parent = parent;
            return parent;
        }
        Logger::ptr child(new Logger(name, log));
        log->m_children.insert(child);
        return child;        
    }
}

void
Log::visit(boost::function<void (boost::shared_ptr<Logger>)> dg)
{
    std::list<Logger::ptr> toVisit;
    toVisit.push_back(root());
    while (!toVisit.empty())
    {
        Logger::ptr cur = toVisit.front();
        toVisit.pop_front();
        dg(cur);
        for (std::set<Logger::ptr, LoggerLess>::iterator it = cur->m_children.begin();
            it != cur->m_children.end();
            ++it) {
            toVisit.push_back(*it);
        }
    }
}

void
Log::addSink(LogSink::ptr sink)
{
    root()->addSink(sink);
}

void
Log::removeSink(LogSink::ptr sink)
{
    root()->removeSink(sink);
}

void
Log::clearSinks()
{
    root()->clearSinks();
}

bool
LoggerLess::operator ()(const Logger::ptr &lhs,
    const Logger::ptr &rhs) const
{
    return lhs->m_name < rhs->m_name;
}


Logger::Logger()
: m_name(":"),
  m_inheritSinks(false)
{}

Logger::Logger(const std::string &name, Logger::ptr parent)
: m_name(name),
  m_parent(parent),
  m_level(Log::INFO),
  m_inheritSinks(true)
{}

void
Logger::level(Log::Level level, bool propagate)
{
    m_level = level;
    if (propagate) {
        for (std::set<Logger::ptr, LoggerLess>::iterator it(m_children.begin());
            it != m_children.end();
            ++it) {
            (*it)->level(level);
        }
    }
}

void
Logger::removeSink(LogSink::ptr sink)
{
    std::list<LogSink::ptr>::iterator it =
        std::find(m_sinks.begin(), m_sinks.end(), sink);
    if (it != m_sinks.end())
        m_sinks.erase(it);
}

void
Logger::log(Log::Level level, const std::string &str,
    const char *file, int line)
{
    if (str.empty() || !enabled(level))
        return;
    // TODO: capture timestamp
    Logger::ptr _this = shared_from_this();
#ifdef WINDOWS
    DWORD thread = GetCurrentThreadId();
#else
    pid_t thread = getpid();
#endif
    void *fiber = Fiber::getThis().get();
    while (_this) {
        for (std::list<LogSink::ptr>::iterator it(_this->m_sinks.begin());
            it != _this->m_sinks.end();
            ++it) {
            (*it)->log(m_name, thread, fiber, level, str, file, line);
        }
        if (!_this->m_inheritSinks)
            break;
        _this = m_parent.lock();
    }
    if (level == Log::FATAL)
        MORDOR_THROW_EXCEPTION(Assertion("Fatal error: " + str));
}

LogEvent::~LogEvent()
{
    m_logger->log(m_level, m_os.str(), m_file, m_line);
}

static const char *levelStrs[] = {
    "NONE",
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "TRACE",
    "VERBOSE"
};

std::ostream &operator <<(std::ostream &os, Log::Level level)
{
    MORDOR_ASSERT(level >= Log::FATAL && level <= Log::VERBOSE);
    return os << levelStrs[level];
}

}