#include "core/JavaRunner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace papiro {
namespace {

/*! Locate `name` inside a directory, trying the platform executable suffix. */
QString executableIn(const QString &directory, const QString &name)
{
    if (directory.isEmpty())
        return QString();

    const QDir dir(directory);
    QStringList candidates;
    candidates.append(dir.filePath(name));
#ifdef Q_OS_WIN
    candidates.append(dir.filePath(name + QStringLiteral(".exe")));
#endif

    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isFile() && info.isExecutable())
            return info.absoluteFilePath();
    }
    return QString();
}

/*! Replace comments and literal contents with spaces so they cannot match. */
QString stripCommentsAndLiterals(const QString &source)
{
    QString result;
    result.reserve(source.size());

    enum class State {
        Normal,
        LineComment,
        BlockComment,
        StringLiteral,
        CharacterLiteral
    };
    State state = State::Normal;

    for (int i = 0; i < source.size(); ++i) {
        const QChar ch = source.at(i);
        const QChar next = (i + 1 < source.size()) ? source.at(i + 1) : QChar();

        switch (state) {
        case State::Normal:
            if (ch == QLatin1Char('/') && next == QLatin1Char('/')) {
                result.append(QLatin1String("  "));
                ++i;
                state = State::LineComment;
            } else if (ch == QLatin1Char('/') && next == QLatin1Char('*')) {
                result.append(QLatin1String("  "));
                ++i;
                state = State::BlockComment;
            } else if (ch == QLatin1Char('"')) {
                result.append(QLatin1Char(' '));
                state = State::StringLiteral;
            } else if (ch == QLatin1Char('\'')) {
                result.append(QLatin1Char(' '));
                state = State::CharacterLiteral;
            } else {
                result.append(ch);
            }
            break;

        case State::LineComment:
            if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
                result.append(ch);
                state = State::Normal;
            } else {
                result.append(QLatin1Char(' '));
            }
            break;

        case State::BlockComment:
            if (ch == QLatin1Char('*') && next == QLatin1Char('/')) {
                result.append(QLatin1String("  "));
                ++i;
                state = State::Normal;
            } else if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
                result.append(ch);
            } else {
                result.append(QLatin1Char(' '));
            }
            break;

        case State::StringLiteral:
        case State::CharacterLiteral: {
            const QChar delimiter = (state == State::StringLiteral) ? QLatin1Char('"')
                                                                    : QLatin1Char('\'');
            if (ch == QLatin1Char('\\') && i + 1 < source.size()) {
                result.append(QLatin1String("  "));
                ++i;
            } else if (ch == delimiter) {
                result.append(QLatin1Char(' '));
                state = State::Normal;
            } else if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
                // Keep line structure; multiline text blocks stay protected.
                result.append(ch);
            } else {
                result.append(QLatin1Char(' '));
            }
            break;
        }
        }
    }

    return result;
}

bool isJavaKeyword(const QString &identifier)
{
    static const QStringList keywords = {
        QStringLiteral("abstract"), QStringLiteral("assert"), QStringLiteral("boolean"),
        QStringLiteral("break"), QStringLiteral("byte"), QStringLiteral("case"),
        QStringLiteral("catch"), QStringLiteral("char"), QStringLiteral("class"),
        QStringLiteral("const"), QStringLiteral("continue"), QStringLiteral("default"),
        QStringLiteral("do"), QStringLiteral("double"), QStringLiteral("else"),
        QStringLiteral("enum"), QStringLiteral("extends"), QStringLiteral("final"),
        QStringLiteral("finally"), QStringLiteral("float"), QStringLiteral("for"),
        QStringLiteral("goto"), QStringLiteral("if"), QStringLiteral("implements"),
        QStringLiteral("import"), QStringLiteral("instanceof"), QStringLiteral("int"),
        QStringLiteral("interface"), QStringLiteral("long"), QStringLiteral("native"),
        QStringLiteral("new"), QStringLiteral("package"), QStringLiteral("permits"),
        QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"),
        QStringLiteral("record"), QStringLiteral("return"), QStringLiteral("sealed"),
        QStringLiteral("short"), QStringLiteral("static"), QStringLiteral("strictfp"),
        QStringLiteral("super"), QStringLiteral("switch"), QStringLiteral("synchronized"),
        QStringLiteral("this"), QStringLiteral("throw"), QStringLiteral("throws"),
        QStringLiteral("transient"), QStringLiteral("try"), QStringLiteral("var"),
        QStringLiteral("void"), QStringLiteral("volatile"), QStringLiteral("while"),
        QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null"),
        QStringLiteral("yield")
    };
    return keywords.contains(identifier);
}

QString sanitizeIdentifier(const QString &candidate)
{
    QString result;
    result.reserve(candidate.size());

    for (const QChar ch : candidate) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('$'))
            result.append(ch);
        else
            result.append(QLatin1Char('_'));
    }

    if (result.isEmpty())
        result = QStringLiteral("Main");

    const QChar first = result.at(0);
    if (!(first.isLetter() || first == QLatin1Char('_') || first == QLatin1Char('$')))
        result.prepend(QLatin1Char('_'));

    if (isJavaKeyword(result))
        result.prepend(QLatin1Char('_'));

    return result;
}

} // namespace

JavaEnvironment JavaRunner::detect()
{
    JavaEnvironment environment;

    const QString javaHome = qEnvironmentVariable("JAVA_HOME").trimmed();
    const QString javaHomeBin = javaHome.isEmpty()
        ? QString()
        : QDir(javaHome).absoluteFilePath(QStringLiteral("bin"));

    QString javaPath = executableIn(javaHomeBin, QStringLiteral("java"));
    QString javacPath = executableIn(javaHomeBin, QStringLiteral("javac"));

    // Fall back to PATH independently for each tool, so a partial JAVA_HOME
    // still yields a usable toolchain when the missing half is on PATH.
    if (javaPath.isEmpty())
        javaPath = QStandardPaths::findExecutable(QStringLiteral("java"));
    if (javacPath.isEmpty())
        javacPath = QStandardPaths::findExecutable(QStringLiteral("javac"));

    environment.javaPath = javaPath;
    environment.javacPath = javacPath;

    if (javaPath.isEmpty()) {
        environment.error = tr("Java runtime (java) was not found in JAVA_HOME or PATH.");
        return environment;
    }

    if (javacPath.isEmpty()) {
        environment.error = tr("Java compiler (javac) was not found. A JDK is required, not just a JRE.");
        return environment;
    }

    environment.available = true;

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(javaPath, QStringList{QStringLiteral("-version")});
    if (!process.waitForStarted(1000)) {
        environment.available = false;
        environment.error = tr("Java runtime at %1 could not be started: %2")
                                .arg(javaPath, process.errorString());
        return environment;
    }

    if (!process.waitForFinished(2000)) {
        process.kill();
        process.waitForFinished(200);
    }
    const QString output = QString::fromUtf8(process.readAll());

    static const QRegularExpression versionExpression(
        QStringLiteral("\"[^\"\\r\\n]*?(\\d+(?:\\.\\d+)+(?:[._+\\-][A-Za-z0-9]+)*)\""));
    const QRegularExpressionMatch match = versionExpression.match(output);
    if (match.hasMatch())
        environment.version = match.captured(1);

    return environment;
}

bool JavaRunner::isJavaFile(const QString &path)
{
    return QFileInfo(path).suffix().compare(QStringLiteral("java"), Qt::CaseInsensitive) == 0;
}

QString JavaRunner::extractClassName(const QString &source, const QString &filePath)
{
    const QString cleaned = stripCommentsAndLiterals(source);

    static const QRegularExpression publicType(
        QStringLiteral("\\bpublic\\s+"
                       "(?:(?:final|abstract|sealed|non-sealed|strictfp"
                       "|@[\\w.$]+(?:\\s*\\([^)]*\\))?)\\s+)*"
                       "(?:class|interface|enum|record)\\s+"
                       "([A-Za-z_$][A-Za-z0-9_$]*)"));
    static const QRegularExpression anyType(
        QStringLiteral("\\b(?:class|interface|enum|record)\\s+"
                       "([A-Za-z_$][A-Za-z0-9_$]*)"));

    QRegularExpressionMatch match = publicType.match(cleaned);
    if (!match.hasMatch())
        match = anyType.match(cleaned);
    if (match.hasMatch())
        return match.captured(1);

    return sanitizeIdentifier(QFileInfo(filePath).completeBaseName());
}

JavaRunner::JavaRunner(QObject *parent)
    : QObject(parent)
{
}

JavaRunner::~JavaRunner()
{
    if (m_process) {
        m_process->disconnect(this);
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(2000);
        }
        m_process = nullptr;
    }
    m_stage = Stage::Idle;
    m_tempDir.reset();
}

void JavaRunner::compileAndRun(const QString &sourcePath, const QStringList &args)
{
    if (m_stage != Stage::Idle)
        cancel();

    m_stage = Stage::Idle;
    m_tempDir.reset();
    m_arguments = args;
    m_sourcePath = QFileInfo(sourcePath).absoluteFilePath();
    m_className.clear();

    const QFileInfo sourceInfo(m_sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        emit finished(false, tr("Source file not found: %1").arg(sourcePath));
        return;
    }

    QFile sourceFile(m_sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        emit finished(false, tr("Could not read source file: %1").arg(sourcePath));
        return;
    }
    const QString source = QString::fromUtf8(sourceFile.readAll());
    m_className = extractClassName(source, m_sourcePath);

    m_environment = detect();
    if (!m_environment.available) {
        emit finished(false, tr("Java toolchain unavailable: %1").arg(m_environment.error));
        return;
    }

    m_tempDir = std::make_unique<QTemporaryDir>();
    if (!m_tempDir->isValid()) {
        emit finished(false, tr("Could not create a temporary directory for the Java build."));
        return;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyRead, this, &JavaRunner::readOutput);
    connect(m_process, &QProcess::finished, this, &JavaRunner::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &JavaRunner::onProcessError);

    startCompile();
}

void JavaRunner::startCompile()
{
    if (!m_process || !m_tempDir)
        return;

    m_stage = Stage::Compiling;
    m_process->setWorkingDirectory(QFileInfo(m_sourcePath).absolutePath());
    m_process->start(m_environment.javacPath,
                     QStringList{QStringLiteral("-encoding"), QStringLiteral("UTF-8"),
                                 QStringLiteral("-d"), m_tempDir->path(), m_sourcePath});
}

void JavaRunner::startRun()
{
    if (!m_process || !m_tempDir)
        return;

    m_stage = Stage::Running;
    QStringList arguments;
    arguments.append(QStringLiteral("-cp"));
    arguments.append(m_tempDir->path());
    arguments.append(m_className);
    arguments.append(m_arguments);
    m_process->start(m_environment.javaPath, arguments);
}

void JavaRunner::readOutput()
{
    if (!m_process)
        return;

    const QByteArray standardOutput = m_process->readAllStandardOutput();
    if (!standardOutput.isEmpty())
        emit outputReady(QString::fromUtf8(standardOutput), false);

    const QByteArray standardError = m_process->readAllStandardError();
    if (!standardError.isEmpty())
        emit outputReady(QString::fromUtf8(standardError), true);
}

void JavaRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_process || m_stage == Stage::Idle)
        return;

    readOutput();

    if (m_stage == Stage::Compiling) {
        if (status != QProcess::NormalExit || exitCode != 0) {
            const QString summary = (status == QProcess::CrashExit)
                ? tr("Compilation failed (compiler crashed)")
                : tr("Compilation failed (exit code %1)").arg(exitCode);
            settle(false, summary);
            return;
        }

        startRun();
        return;
    }

    const bool success = (status == QProcess::NormalExit && exitCode == 0);
    const QString summary = (status == QProcess::CrashExit)
        ? tr("Program crashed")
        : tr("Finished with exit code %1").arg(exitCode);
    settle(success, summary);
}

void JavaRunner::onProcessError(QProcess::ProcessError error)
{
    if (!m_process || m_stage == Stage::Idle)
        return;

    if (error == QProcess::FailedToStart) {
        const QString tool = (m_stage == Stage::Compiling)
            ? QStringLiteral("javac")
            : QStringLiteral("java");
        settle(false, tr("Failed to start %1: %2").arg(tool, m_process->errorString()));
    }
}

void JavaRunner::cancel()
{
    if (m_stage == Stage::Idle)
        return;

    if (m_process) {
        // Ignore any pending QProcess signals so cancellation always produces
        // a single "Cancelled" result.
        m_process->disconnect(this);
        readOutput();
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(2000);
        }
        readOutput();
    }

    settle(false, tr("Cancelled"));
}

bool JavaRunner::isRunning() const
{
    return m_stage != Stage::Idle && m_process
        && m_process->state() != QProcess::NotRunning;
}

void JavaRunner::settle(bool success, const QString &summary)
{
    if (m_stage == Stage::Idle)
        return;

    m_stage = Stage::Idle;
    if (m_process) {
        m_process->disconnect(this);
        m_process->deleteLater();
        m_process = nullptr;
    }

    emit finished(success, summary);
}

} // namespace papiro
