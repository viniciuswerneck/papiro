#include "core/ClipLibrary.h"
#include "core/Encoding.h"
#include "core/Macro.h"
#include "core/SyntaxDefinition.h"
#include "core/Workspace.h"

#include <QScopedPointer>
#include <QTemporaryDir>
#include <QtTest>

using namespace papiro;

namespace papiro {
// MacroManager::playMacro() lives in the GUI-free core but delegates the actual
// playback to Editor.cpp. The core tests provide a no-op implementation so the
// library can be linked without the editor widget.
bool executeMacroAction(const MacroAction &, QPlainTextEdit *)
{
    return false;
}
} // namespace papiro

class TestCore : public QObject {
    Q_OBJECT

private slots:
    void encodingDetection_data();
    void encodingDetection();
    void decodeNormalizesLineEndings();
    void encodingRoundTrip_data();
    void encodingRoundTrip();
    void syntaxDefinitionParsing();
    void syntaxDefinitionMatchesFile();
    void syntaxDefinitionReportsErrors();
    void macroCoalescesActions();
    void macroFileRoundTrip();
    void macroTextFormatRoundTrip();
    void macroManagerPersistsSlots();
    void workspaceRoundTrip();
    void clipLibraryBasics();
};

void TestCore::encodingDetection_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<int>("expected");

    QTest::newRow("utf8") << QByteArray("hello") << int(TextEncoding::Utf8);
    QTest::newRow("utf8-bom") << QByteArray("\xEF\xBB\xBFhello", 8)
                              << int(TextEncoding::Utf8Bom);
    QTest::newRow("utf16-le") << QByteArray("\xFF\xFEh\x00i\x00", 6)
                              << int(TextEncoding::Utf16LE);
    QTest::newRow("utf16-be") << QByteArray("\xFE\xFF\x00h\x00i", 6)
                              << int(TextEncoding::Utf16BE);
    QTest::newRow("windows-1252") << QByteArray("caf\xE9", 4)
                                  << int(TextEncoding::Windows1252);
}

void TestCore::encodingDetection()
{
    QFETCH(QByteArray, data);
    QFETCH(int, expected);

    QCOMPARE(int(detectEncoding(data)), expected);
}

void TestCore::decodeNormalizesLineEndings()
{
    const DecodedFile decoded = decodeFile(QByteArray("a\r\nb\rc\n"));
    QCOMPARE(decoded.text, QStringLiteral("a\nb\nc\n"));
    QCOMPARE(decoded.eol, LineEnding::CrLf);
    QCOMPARE(decoded.encoding, TextEncoding::Utf8);

    const DecodedFile bom = decodeFile(QByteArray("\xEF\xBB\xBFtext", 7));
    QCOMPARE(bom.text, QStringLiteral("text"));
    QCOMPARE(bom.encoding, TextEncoding::Utf8Bom);

    const DecodedFile utf16 = decodeFile(QByteArray("\xFF\xFEh\x00i\x00", 6));
    QCOMPARE(utf16.text, QStringLiteral("hi"));
    QCOMPARE(utf16.encoding, TextEncoding::Utf16LE);

    const DecodedFile win1252 = decodeFile(QByteArray("caf\xE9", 4));
    QCOMPARE(win1252.text, QStringLiteral("café"));
    QCOMPARE(win1252.encoding, TextEncoding::Windows1252);
}

void TestCore::encodingRoundTrip_data()
{
    QTest::addColumn<int>("encoding");
    QTest::addColumn<int>("eol");

    for (TextEncoding encoding : {TextEncoding::Utf8, TextEncoding::Utf8Bom,
                                  TextEncoding::Utf16LE, TextEncoding::Utf16BE,
                                  TextEncoding::Windows1252}) {
        for (LineEnding eol : {LineEnding::Lf, LineEnding::CrLf, LineEnding::Cr}) {
            QTest::addRow("enc%d-eol%d", int(encoding), int(eol))
                << int(encoding) << int(eol);
        }
    }
}

void TestCore::encodingRoundTrip()
{
    QFETCH(int, encoding);
    QFETCH(int, eol);

    const QString text = QStringLiteral("primeira linha\nsegunda linha çã");
    const QByteArray encoded = encodeFile(text, TextEncoding(encoding), LineEnding(eol));
    const DecodedFile decoded = decodeFile(encoded);

    QCOMPARE(decoded.text, text);
    QCOMPARE(decoded.encoding, TextEncoding(encoding));
    QCOMPARE(decoded.eol, LineEnding(eol));
}

void TestCore::syntaxDefinitionParsing()
{
    const QString source = QStringLiteral(
        "[Demo]\n"
        "; comment line\n"
        "Name=Demo Lang\n"
        "Description=A test language\n"
        "Extensions=.demo;.dm\n"
        "CaseSensitive=Yes\n"
        "Keywords=alpha beta gamma\n"
        "LineComment=#\n"
        "BlockCommentStart=/*\n"
        "BlockCommentEnd=*/\n"
        "StringDelimiters=' \"\n"
        "MultilineStringDelimiters=```\n"
        "EscapeChar=\\\n"
        "Numbers=Off\n");

    QString error;
    const SyntaxDefinition definition = SyntaxDefinition::fromText(source, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QCOMPARE(definition.name, QStringLiteral("Demo Lang"));
    QCOMPARE(definition.description, QStringLiteral("A test language"));
    QCOMPARE(definition.extensions,
             QStringList({QStringLiteral(".demo"), QStringLiteral(".dm")}));
    QVERIFY(definition.caseSensitive);
    QCOMPARE(definition.keywords, QStringList({"alpha", "beta", "gamma"}));
    QCOMPARE(definition.lineComment, QStringLiteral("#"));
    QCOMPARE(definition.blockCommentStart, QStringLiteral("/*"));
    QCOMPARE(definition.blockCommentEnd, QStringLiteral("*/"));
    QCOMPARE(definition.stringDelimiters.size(), 2);
    QCOMPARE(definition.multilineStringDelimiters, QStringList({QStringLiteral("```")}));
    QCOMPARE(definition.escapeChar, QStringLiteral("\\"));
    QVERIFY(!definition.highlightNumbers);
}

void TestCore::syntaxDefinitionMatchesFile()
{
    SyntaxDefinition definition;
    definition.extensions = {QStringLiteral(".cpp"), QStringLiteral("h")};

    QVERIFY(definition.matchesFile(QStringLiteral("main.cpp")));
    QVERIFY(definition.matchesFile(QStringLiteral("MAIN.CPP")));
    QVERIFY(definition.matchesFile(QStringLiteral("header.h")));
    QVERIFY(!definition.matchesFile(QStringLiteral("main.hpp")));
    QVERIFY(!definition.matchesFile(QStringLiteral("cpp")));
}

void TestCore::syntaxDefinitionReportsErrors()
{
    QString error;
    SyntaxDefinition::fromText(QStringLiteral("Name=NoEquals\nnot a pair\n"), &error);
    QVERIFY(!error.isEmpty());
}

void TestCore::macroCoalescesActions()
{
    Macro macro;
    macro.addAction(MacroAction::fromTextInput(QStringLiteral("a")));
    macro.addAction(MacroAction::fromTextInput(QStringLiteral("b")));
    QCOMPARE(macro.actions().size(), 1);
    QCOMPARE(macro.actions().first().type, MacroAction::TextInput);
    QCOMPARE(macro.actions().first().text, QStringLiteral("ab"));

    macro.addAction(MacroAction::fromKeyPress(Qt::Key_Backspace, Qt::NoModifier));
    macro.addAction(MacroAction::fromKeyPress(Qt::Key_Backspace, Qt::NoModifier));
    QCOMPARE(macro.actions().size(), 2);
    QCOMPARE(macro.actions().last().repeat, 2);

    macro.addAction(MacroAction::fromCommand(QStringLiteral("copy")));
    QCOMPARE(macro.actions().size(), 3);
    QCOMPARE(macro.actions().last().command, QStringLiteral("copy"));
}

void TestCore::macroFileRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("macro.tpm"));

    Macro macro;
    macro.setName(QStringLiteral("Test macro"));
    macro.setDescription(QStringLiteral("round trip"));
    macro.addAction(MacroAction::fromTextInput(QStringLiteral("hello")));
    macro.addAction(MacroAction::fromKeyPress(Qt::Key_Return, Qt::ControlModifier));
    macro.addAction(MacroAction::fromCommand(QStringLiteral("selectAll")));

    QString error;
    QVERIFY2(macro.saveToFile(path, &error), qPrintable(error));

    QScopedPointer<Macro> loaded(Macro::loadFromFile(path, &error));
    QVERIFY2(loaded, qPrintable(error));
    QCOMPARE(loaded->name(), macro.name());
    QCOMPARE(loaded->description(), macro.description());
    QCOMPARE(loaded->actions().size(), 3);
    QCOMPARE(loaded->actions().at(0).text, QStringLiteral("hello"));
    QCOMPARE(loaded->actions().at(1).key, int(Qt::Key_Return));
    QVERIFY(loaded->actions().at(1).modifiers == Qt::ControlModifier);
    QCOMPARE(loaded->actions().at(2).command, QStringLiteral("selectAll"));
}

void TestCore::macroTextFormatRoundTrip()
{
    Macro macro;
    macro.setName(QStringLiteral("Text macro"));
    macro.addAction(MacroAction::fromTextInput(QStringLiteral("abc")));
    macro.addAction(MacroAction::fromKeyPress(Qt::Key_Tab, Qt::ShiftModifier));
    macro.addAction(MacroAction::fromCommand(QStringLiteral("paste")));

    QScopedPointer<Macro> loaded(Macro::fromTextFormat(macro.toTextFormat()));
    QVERIFY(loaded);
    QCOMPARE(loaded->actions().size(), 3);
    QCOMPARE(loaded->actions().at(0).text, QStringLiteral("abc"));
    QCOMPARE(loaded->actions().at(1).key, int(Qt::Key_Tab));
    QVERIFY(loaded->actions().at(1).modifiers == Qt::ShiftModifier);
    QCOMPARE(loaded->actions().at(2).command, QStringLiteral("paste"));
}

void TestCore::macroManagerPersistsSlots()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    {
        MacroManager manager;
        manager.startRecording(QStringLiteral("First"));
        manager.recordingMacro()->addAction(MacroAction::fromTextInput(QStringLiteral("one")));
        Macro *first = manager.stopRecording();

        manager.startRecording(QStringLiteral("Second"));
        manager.recordingMacro()->addAction(MacroAction::fromTextInput(QStringLiteral("two")));
        Macro *second = manager.stopRecording();

        manager.setBuiltinMacro(0, first);
        manager.setBuiltinMacro(2, second);
        QVERIFY(manager.saveToDirectory(directory.path()));
    }

    {
        MacroManager manager;
        manager.loadFromDirectory(directory.path());
        QCOMPARE(manager.userMacros().size(), 2);
        QVERIFY(manager.builtinMacro(0));
        QCOMPARE(manager.builtinMacro(0)->name(), QStringLiteral("First"));
        QVERIFY(manager.builtinMacro(2));
        QCOMPARE(manager.builtinMacro(2)->name(), QStringLiteral("Second"));

        manager.userMacros().first()->setName(QStringLiteral("Renamed First"));
        QVERIFY(manager.saveToDirectory(directory.path()));
    }

    {
        MacroManager manager;
        manager.loadFromDirectory(directory.path());
        QCOMPARE(manager.userMacros().size(), 2);
        QCOMPARE(manager.builtinMacro(0)->name(), QStringLiteral("Renamed First"));

        Macro *renamed = manager.userMacros().first();
        QVERIFY(manager.removeUserMacro(renamed));
        QVERIFY(!manager.builtinMacro(0));
        QVERIFY(manager.builtinMacro(2));
        QVERIFY(manager.saveToDirectory(directory.path()));
    }

    {
        MacroManager manager;
        manager.loadFromDirectory(directory.path());
        QCOMPARE(manager.userMacros().size(), 1);
        QCOMPARE(manager.userMacros().first()->name(), QStringLiteral("Second"));
        QVERIFY(!manager.builtinMacro(0));
        QVERIFY(manager.builtinMacro(2));
        QCOMPARE(manager.builtinMacro(2)->name(), QStringLiteral("Second"));
    }
}

void TestCore::workspaceRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("session.ppw"));

    Workspace workspace;
    workspace.name = QStringLiteral("Demo");
    workspace.rootPath = directory.path();
    workspace.files = {QStringLiteral("/tmp/a.txt"), QStringLiteral("/tmp/b.txt")};
    workspace.activeIndex = 1;

    QString error;
    QVERIFY2(workspace.save(path, &error), qPrintable(error));

    const Workspace loaded = Workspace::load(path, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(loaded.isValid());
    QCOMPARE(loaded.name, workspace.name);
    QCOMPARE(loaded.rootPath, workspace.rootPath);
    QCOMPARE(loaded.files, workspace.files);
    QCOMPARE(loaded.activeIndex, 1);
}

void TestCore::clipLibraryBasics()
{
    ClipLibrary library;
    QVERIFY(library.clips().isEmpty());

    const QString id = library.addClip(QStringLiteral("Hello"), QStringLiteral("console.log(hi)"),
                                       QStringLiteral("JavaScript"),
                                       {QStringLiteral("web"), QStringLiteral("log")});
    QVERIFY(!id.isEmpty());
    QCOMPARE(library.clips().size(), 1);
    QVERIFY(library.findClip(id));

    QCOMPARE(library.search(QStringLiteral("console")).size(), 1);
    QCOMPARE(library.search(QStringLiteral("WEB")).size(), 1);
    QCOMPARE(library.search(QStringLiteral("javascript")).size(), 1);
    QVERIFY(library.search(QStringLiteral("missing")).isEmpty());

    QCOMPARE(library.categories(),
             QStringList({QStringLiteral("Uncategorized"), QStringLiteral("JavaScript")}));

    library.recordUsage(id);
    QCOMPARE(library.findClip(id)->useCount, 1);

    QVERIFY(library.updateClip(id, QStringLiteral("Renamed"), QStringLiteral("content"),
                               QStringLiteral("Snippets"), {QStringLiteral("new")}));
    QCOMPARE(library.search(QStringLiteral("Renamed")).size(), 1);
    QCOMPARE(library.findClip(id)->category, QStringLiteral("Snippets"));

    QVERIFY(library.removeClip(id));
    QVERIFY(library.clips().isEmpty());
}

QTEST_GUILESS_MAIN(TestCore)

#include "test_core.moc"
