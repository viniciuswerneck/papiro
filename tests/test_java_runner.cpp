#include "core/JavaRunner.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace papiro;

class TestJavaRunner : public QObject {
    Q_OBJECT

private slots:
    void isJavaFile_data();
    void isJavaFile();
    void extractClassName_data();
    void extractClassName();
    void detectIsSelfConsistent();
    void compileAndRunRoundTrip();
    void compileFailureIsReported();
};

void TestJavaRunner::isJavaFile_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("expected");

    QTest::newRow("lowercase") << QStringLiteral("Main.java") << true;
    QTest::newRow("uppercase") << QStringLiteral("Main.JAVA") << true;
    QTest::newRow("mixed-case") << QStringLiteral("Main.Java") << true;
    QTest::newRow("with-directory") << QStringLiteral("src/com/example/Main.java") << true;
    QTest::newRow("other-extension") << QStringLiteral("Main.txt") << false;
    QTest::newRow("trailing-extension") << QStringLiteral("Main.java.bak") << false;
    QTest::newRow("no-extension") << QStringLiteral("java") << false;
}

void TestJavaRunner::isJavaFile()
{
    QFETCH(QString, path);
    QFETCH(bool, expected);

    QCOMPARE(JavaRunner::isJavaFile(path), expected);
}

void TestJavaRunner::extractClassName_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<QString>("filePath");
    QTest::addColumn<QString>("expected");

    const QString filePath = QStringLiteral("/tmp/Hello.java");
    const QString fallbackPath = QStringLiteral("/tmp/NoDeclaration.java");

    QTest::newRow("public-class")
        << QStringLiteral("public class Hello {}") << filePath << QStringLiteral("Hello");
    QTest::newRow("package-private-class")
        << QStringLiteral("class Hello {}") << filePath << QStringLiteral("Hello");
    QTest::newRow("public-final-extends")
        << QStringLiteral("public final class Hello extends Base {}") << filePath
        << QStringLiteral("Hello");
    QTest::newRow("public-interface")
        << QStringLiteral("public interface Hello {}") << filePath << QStringLiteral("Hello");
    QTest::newRow("enum")
        << QStringLiteral("enum Color { RED, GREEN }") << filePath << QStringLiteral("Color");
    QTest::newRow("public-record")
        << QStringLiteral("public record Point(int x, int y) {}") << filePath
        << QStringLiteral("Point");
    QTest::newRow("package-declaration")
        << QStringLiteral("package com.example.app;\n\npublic class Hello {}") << filePath
        << QStringLiteral("Hello");
    QTest::newRow("line-and-block-comments")
        << QStringLiteral("// public class Fake\n/* class AlsoFake */\npublic class Hello {}")
        << filePath << QStringLiteral("Hello");
    QTest::newRow("whitespace")
        << QStringLiteral("public\n    final\n\tclass   Hello\n    extends Base {}") << filePath
        << QStringLiteral("Hello");
    QTest::newRow("annotation")
        << QStringLiteral("@Deprecated\npublic class Hello {}") << filePath
        << QStringLiteral("Hello");
    QTest::newRow("class-name-in-string")
        << QStringLiteral("class Hello { String s = \"public class Fake\"; }") << filePath
        << QStringLiteral("Hello");
    QTest::newRow("class-name-in-text-block")
        << QStringLiteral("class Hello {\n    String s = \"\"\"\n        public class Fake\n"
                          "        \"\"\";\n}")
        << filePath << QStringLiteral("Hello");
    QTest::newRow("outer-class-before-inner")
        << QStringLiteral("public class Outer { static class Inner {} }") << filePath
        << QStringLiteral("Outer");
    QTest::newRow("no-declaration-fallback")
        << QStringLiteral("int value = 42;") << fallbackPath
        << QStringLiteral("NoDeclaration");
    QTest::newRow("sanitized-fallback")
        << QString() << QStringLiteral("/tmp/my-file name.java") << QStringLiteral("my_file_name");
    QTest::newRow("leading-digit-fallback")
        << QString() << QStringLiteral("/tmp/123Main.java") << QStringLiteral("_123Main");
    QTest::newRow("keyword-fallback")
        << QString() << QStringLiteral("/tmp/class.java") << QStringLiteral("_class");
    QTest::newRow("empty-file-name")
        << QStringLiteral("int value = 1;") << QString() << QStringLiteral("Main");
}

void TestJavaRunner::extractClassName()
{
    QFETCH(QString, source);
    QFETCH(QString, filePath);
    QFETCH(QString, expected);

    QCOMPARE(JavaRunner::extractClassName(source, filePath), expected);
}

void TestJavaRunner::detectIsSelfConsistent()
{
    QElapsedTimer timer;
    timer.start();
    const JavaEnvironment environment = JavaRunner::detect();
    const qint64 elapsed = timer.elapsed();

    QVERIFY2(elapsed < 5000, qPrintable(QStringLiteral("detect() took %1 ms").arg(elapsed)));

    if (environment.available) {
        QVERIFY(!environment.javaPath.isEmpty());
        QVERIFY(!environment.javacPath.isEmpty());
        QVERIFY(QFileInfo(environment.javaPath).isAbsolute());
        QVERIFY(QFileInfo(environment.javacPath).isAbsolute());
    } else {
        QVERIFY(!environment.error.isEmpty());
    }
}

void TestJavaRunner::compileAndRunRoundTrip()
{
    const JavaEnvironment environment = JavaRunner::detect();
    if (!environment.available)
        QSKIP("No JDK available for the compile and run round trip.");

    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());
    const QString sourcePath = sourceDir.filePath(QStringLiteral("Main.java"));

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray code =
        "public class Main {\n"
        "    public static void main(String[] args) {\n"
        "        System.out.println(\"Hello from Java \" + args[0]);\n"
        "        System.err.println(\"stderr-line\");\n"
        "    }\n"
        "}\n";
    QCOMPARE(source.write(code), code.size());
    source.close();

    JavaRunner runner;
    QSignalSpy outputSpy(&runner, &JavaRunner::outputReady);
    QSignalSpy finishedSpy(&runner, &JavaRunner::finished);

    runner.compileAndRun(sourcePath, QStringList{QStringLiteral("world")});

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 90000);
    QCOMPARE(finishedSpy.count(), 1);

    const QList<QVariant> result = finishedSpy.first();
    const bool success = result.at(0).toBool();
    const QString summary = result.at(1).toString();
    QVERIFY2(success, qPrintable(summary));
    QVERIFY2(summary.contains(QStringLiteral("Finished with exit code 0")),
             qPrintable(summary));

    QString output;
    bool sawErrorChannel = false;
    for (const QList<QVariant> &emission : outputSpy) {
        output += emission.at(0).toString();
        sawErrorChannel = sawErrorChannel || emission.at(1).toBool();
    }

    QVERIFY2(output.contains(QStringLiteral("Hello from Java world")), qPrintable(output));
    QVERIFY2(output.contains(QStringLiteral("stderr-line")), qPrintable(output));
    QVERIFY(sawErrorChannel);

    QTest::qWait(200);
    QCOMPARE(finishedSpy.count(), 1);
}

void TestJavaRunner::compileFailureIsReported()
{
    const JavaEnvironment environment = JavaRunner::detect();
    if (!environment.available)
        QSKIP("No JDK available for the compile failure test.");

    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());
    const QString sourcePath = sourceDir.filePath(QStringLiteral("Broken.java"));

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray code = "public class Broken { this is not valid java }\n";
    QCOMPARE(source.write(code), code.size());
    source.close();

    JavaRunner runner;
    QSignalSpy outputSpy(&runner, &JavaRunner::outputReady);
    QSignalSpy finishedSpy(&runner, &JavaRunner::finished);

    runner.compileAndRun(sourcePath);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 90000);
    QCOMPARE(finishedSpy.count(), 1);

    const QList<QVariant> result = finishedSpy.first();
    QVERIFY(!result.at(0).toBool());
    const QString summary = result.at(1).toString();
    QVERIFY2(summary.contains(QStringLiteral("Compilation failed")), qPrintable(summary));
    QVERIFY2(summary.contains(QStringLiteral("exit code")), qPrintable(summary));

    QString output;
    for (const QList<QVariant> &emission : outputSpy)
        output += emission.at(0).toString();
    QVERIFY2(output.contains(QStringLiteral("error:")), qPrintable(output));

    QTest::qWait(200);
    QCOMPARE(finishedSpy.count(), 1);
}

QTEST_GUILESS_MAIN(TestJavaRunner)

#include "test_java_runner.moc"
