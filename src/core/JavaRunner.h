#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <memory>

class QTemporaryDir;

namespace papiro {

/*! Result of probing the system for a Java toolchain. */
struct JavaEnvironment {
    bool available = false;
    QString javaPath;  // absolute path to `java`
    QString javacPath; // absolute path to `javac`
    QString version;   // e.g. "17.0.10" parsed from `java -version`
    QString error;     // human-readable reason when unavailable
};

/*! Detects Java, compiles a single source file and streams its execution. */
class JavaRunner : public QObject {
    Q_OBJECT

public:
    /*! Look for JAVA_HOME/bin first, then PATH. Never blocks for long. */
    static JavaEnvironment detect();

    /*! True for paths ending in .java (case-insensitive). */
    static bool isJavaFile(const QString &path);

    /*! Public type declaration name, otherwise a sanitized file base name. */
    static QString extractClassName(const QString &source, const QString &filePath);

    explicit JavaRunner(QObject *parent = nullptr);
    ~JavaRunner() override;

    /*! Compile sourcePath into a temporary directory, then run the class. */
    void compileAndRun(const QString &sourcePath,
                       const QStringList &args = QStringList());

    /*! Kill the compile/run process and settle the current invocation. */
    void cancel();

    bool isRunning() const;

signals:
    void outputReady(const QString &text, bool isError);
    void finished(bool success, const QString &summary);

private:
    enum class Stage {
        Idle,
        Compiling,
        Running
    };

    void startCompile();
    void startRun();
    void readOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void settle(bool success, const QString &summary);

    QProcess *m_process = nullptr;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    JavaEnvironment m_environment;
    QString m_sourcePath;
    QStringList m_arguments;
    QString m_className;
    Stage m_stage = Stage::Idle;
};

} // namespace papiro
