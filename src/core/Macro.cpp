#include "core/Macro.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>

namespace papiro {

// ---------------------------------------------------------------------------
// MacroAction
// ---------------------------------------------------------------------------

MacroAction MacroAction::fromTextInput(const QString &text)
{
    MacroAction action;
    action.type = TextInput;
    action.text = text;
    return action;
}

MacroAction MacroAction::fromKeyPress(int key, Qt::KeyboardModifiers modifiers)
{
    MacroAction action;
    action.type = KeyPress;
    action.key = key;
    action.modifiers = modifiers;
    return action;
}

MacroAction MacroAction::fromCommand(const QString &command)
{
    MacroAction action;
    action.type = EditorCommand;
    action.command = command;
    return action;
}

// ---------------------------------------------------------------------------
// Macro
// ---------------------------------------------------------------------------

Macro::Macro(QObject *parent)
    : QObject(parent)
    , m_created(QDateTime::currentDateTime())
    , m_modified(m_created)
{
}

void Macro::setShortcut(const QKeySequence &shortcut)
{
    if (m_shortcut != shortcut) {
        m_shortcut = shortcut;
        m_modified = QDateTime::currentDateTime();
        emit shortcutChanged(shortcut);
    }
}

void Macro::addAction(const MacroAction &action)
{
    // Coalesce consecutive text input actions
    if (action.type == MacroAction::TextInput && !m_actions.isEmpty()) {
        MacroAction &last = m_actions.last();
        if (last.type == MacroAction::TextInput && last.repeat == 1) {
            last.text += action.text;
            m_modified = QDateTime::currentDateTime();
            return;
        }
    }
    // Coalesce repeated identical key presses (e.g., multiple Backspace)
    if (action.type == MacroAction::KeyPress && !m_actions.isEmpty()) {
        MacroAction &last = m_actions.last();
        if (last.type == MacroAction::KeyPress && last.key == action.key
            && last.modifiers == action.modifiers && last.repeat < 1000) {
            ++last.repeat;
            m_modified = QDateTime::currentDateTime();
            return;
        }
    }
    m_actions.append(action);
    m_modified = QDateTime::currentDateTime();
}

void Macro::clear()
{
    m_actions.clear();
    m_modified = QDateTime::currentDateTime();
}

int Macro::estimatedDurationMs() const
{
    // Very rough: ~30ms per action + 5ms per char for text input
    int ms = 0;
    for (const MacroAction &action : m_actions) {
        switch (action.type) {
        case MacroAction::TextInput:
            ms += 5 * action.text.size() + 30;
            break;
        case MacroAction::KeyPress:
            ms += 30 * action.repeat;
            break;
        case MacroAction::EditorCommand:
            ms += 40;
            break;
        case MacroAction::CursorMovement:
            ms += 20;
            break;
        }
    }
    return ms;
}

// JSON serialization format (version 1)
static constexpr int MacroFileVersion = 1;

bool Macro::saveToFile(const QString &path, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = tr("Cannot open %1 for writing: %2").arg(path, file.errorString());
        return false;
    }

    QJsonObject root;
    root["version"] = MacroFileVersion;
    root["name"] = m_name;
    root["description"] = m_description;
    root["created"] = m_created.toString(Qt::ISODate);
    root["modified"] = m_modified.toString(Qt::ISODate);

    QJsonArray actionsArray;
    for (const MacroAction &action : m_actions) {
        QJsonObject a;
        a["type"] = static_cast<int>(action.type);
        if (action.type == MacroAction::TextInput)
            a["text"] = action.text;
        else if (action.type == MacroAction::KeyPress) {
            a["key"] = action.key;
            a["modifiers"] = static_cast<int>(action.modifiers);
        } else if (action.type == MacroAction::EditorCommand)
            a["command"] = action.command;
        else if (action.type == MacroAction::CursorMovement)
            a["cursor"] = true; // placeholder
        if (action.repeat > 1)
            a["repeat"] = action.repeat;
        actionsArray.append(a);
    }
    root["actions"] = actionsArray;

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

Macro *Macro::loadFromFile(const QString &path, QString *error, QObject *parent)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = tr("Cannot open %1 for reading: %2").arg(path, file.errorString());
        return nullptr;
    }

    QByteArray data = file.readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = tr("Invalid JSON in %1: %2").arg(path, parseError.errorString());
        return nullptr;
    }

    QJsonObject root = doc.object();
    int version = root["version"].toInt(0);
    if (version != MacroFileVersion) {
        if (error) *error = tr("Unsupported macro file version %1 (expected %2)").arg(version).arg(MacroFileVersion);
        return nullptr;
    }

    auto *macro = new Macro(parent);
    macro->m_name = root["name"].toString();
    macro->m_description = root["description"].toString();
    macro->m_created = QDateTime::fromString(root["created"].toString(), Qt::ISODate);
    macro->m_modified = QDateTime::fromString(root["modified"].toString(), Qt::ISODate);

    QJsonArray actionsArray = root["actions"].toArray();
    for (const QJsonValue &val : actionsArray) {
        QJsonObject a = val.toObject();
        MacroAction action;
        action.type = static_cast<MacroAction::Type>(a["type"].toInt(0));
        action.repeat = a["repeat"].toInt(1);
        if (action.type == MacroAction::TextInput)
            action.text = a["text"].toString();
        else if (action.type == MacroAction::KeyPress) {
            action.key = a["key"].toInt(0);
            action.modifiers = static_cast<Qt::KeyboardModifiers>(a["modifiers"].toInt(0));
        } else if (action.type == MacroAction::EditorCommand)
            action.command = a["command"].toString();
        macro->m_actions.append(action);
    }
    return macro;
}

// Human-readable text format (for debugging / .TPM-like export)
Macro *Macro::fromTextFormat(const QString &text, QString *error, QObject *parent)
{
    auto *macro = new Macro(parent);
    QTextStream stream(const_cast<QString *>(&text)); // const_cast safe: read-only
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';'))
            continue;
        // Format: TYPE:DATA
        int colon = line.indexOf(':');
        if (colon <= 0) continue;
        QString type = line.left(colon).toLower();
        QString data = line.mid(colon + 1);
        if (type == "text") {
            macro->addAction(MacroAction::fromTextInput(data));
        } else if (type == "key") {
            // key:Qt::Key_Return:ControlModifier
            QStringList parts = data.split(':');
            if (parts.size() >= 1) {
                int key = parts[0].toInt();
                Qt::KeyboardModifiers mods = Qt::NoModifier;
                if (parts.size() >= 2)
                    mods = static_cast<Qt::KeyboardModifiers>(parts[1].toInt());
                macro->addAction(MacroAction::fromKeyPress(key, mods));
            }
        } else if (type == "cmd") {
            macro->addAction(MacroAction::fromCommand(data));
        }
    }
    return macro;
}

QString Macro::toTextFormat() const
{
    QString out;
    QTextStream stream(&out);
    stream << "# Papiro Macro: " << m_name << "\n";
    stream << "# Created: " << m_created.toString(Qt::ISODate) << "\n";
    for (const MacroAction &action : m_actions) {
        switch (action.type) {
        case MacroAction::TextInput:
            // Escape newlines
            {
                QString escaped = action.text;
                escaped.replace('\n', "\\n").replace('\r', "\\r").replace('\t', "\\t");
                stream << "text:" << escaped << "\n";
            }
            break;
        case MacroAction::KeyPress:
            stream << "key:" << action.key << ":" << static_cast<int>(action.modifiers) << "\n";
            break;
        case MacroAction::EditorCommand:
            stream << "cmd:" << action.command << "\n";
            break;
        case MacroAction::CursorMovement:
            break;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// MacroManager
// ---------------------------------------------------------------------------

MacroManager::MacroManager(QObject *parent)
    : QObject(parent)
{
    for (int i = 0; i < MaxBuiltinMacros; ++i)
        m_builtins[i] = nullptr;
}

void MacroManager::startRecording(const QString &name)
{
    if (m_recording) {
        cancelRecording();
    }
    m_recording = new Macro(this);
    if (!name.isEmpty())
        m_recording->setName(name);
    else
        m_recording->setName(tr("Macro %1").arg(m_userMacros.size() + 1 + MaxBuiltinMacros));
    emit recordingStarted(m_recording);
}

Macro *MacroManager::stopRecording()
{
    if (!m_recording)
        return nullptr;
    Macro *finished = m_recording;
    m_recording = nullptr;
    m_userMacros.append(finished);
    emit recordingStopped(finished);
    emit macroListChanged();
    return finished;
}

void MacroManager::cancelRecording()
{
    if (!m_recording)
        return;
    m_recording->deleteLater();
    m_recording = nullptr;
}

void MacroManager::playMacro(Macro *macro, QPlainTextEdit *editor, int repeatCount)
{
    if (!macro || !editor)
        return;
    for (int r = 0; r < repeatCount; ++r) {
        for (const MacroAction &action : macro->actions()) {
            for (int rep = 0; rep < action.repeat; ++rep) {
                if (!executeMacroAction(action, editor)) {
                    // Action failed; continue with next
                }
                // Small delay to avoid overwhelming the event loop
                QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
            }
        }
    }
}

void MacroManager::playBuiltin(int index, QPlainTextEdit *editor, int repeatCount)
{
    if (index < 0 || index >= MaxBuiltinMacros)
        return;
    if (m_builtins[index])
        playMacro(m_builtins[index], editor, repeatCount);
}

void MacroManager::setBuiltinMacro(int index, Macro *macro)
{
    if (index < 0 || index >= MaxBuiltinMacros)
        return;
    m_builtins[index] = macro;
    if (macro) {
        macro->setParent(this);
        if (macro != m_recording && !m_userMacros.contains(macro))
            m_userMacros.append(macro);
    }
    emit macroListChanged();
}

void MacroManager::clearBuiltinMacro(int index)
{
    if (index < 0 || index >= MaxBuiltinMacros)
        return;
    if (!m_builtins[index])
        return;
    m_builtins[index] = nullptr;
    emit macroListChanged();
}

bool MacroManager::removeUserMacro(Macro *macro)
{
    if (!macro)
        return false;

    if (macro == m_recording) {
        cancelRecording();
        return true;
    }

    for (int i = 0; i < MaxBuiltinMacros; ++i) {
        if (m_builtins[i] == macro)
            m_builtins[i] = nullptr;
    }

    const bool removed = m_userMacros.removeOne(macro);
    if (removed) {
        macro->deleteLater();
        emit macroListChanged();
    }
    return removed;
}

Macro *MacroManager::builtinMacro(int index) const
{
    if (index < 0 || index >= MaxBuiltinMacros)
        return nullptr;
    return m_builtins[index];
}

void MacroManager::loadFromDirectory(const QString &dir)
{
    QDir directory(dir);
    if (!directory.exists())
        return;

    QHash<QString, Macro *> macrosByFile;
    const QStringList files = directory.entryList({"*.tpm"}, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        QString error;
        Macro *macro = Macro::loadFromFile(directory.filePath(file), &error, this);
        if (macro) {
            m_userMacros.append(macro);
            macrosByFile.insert(file, macro);
        }
    }

    // Restore built-in slot assignments and custom shortcuts saved next to the macro files.
    QFile manifest(directory.filePath(QStringLiteral("slots.json")));
    if (manifest.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(manifest.readAll());
        if (document.isObject()) {
            const QJsonObject root = document.object();
            const int version = root.value(QStringLiteral("version")).toInt(1);

            const QJsonArray slotEntries = root.value(QStringLiteral("slots")).toArray();
            for (const QJsonValue &value : slotEntries) {
                const QJsonObject entry = value.toObject();
                const int index = entry.value(QStringLiteral("slot")).toInt(-1);
                const QString file = entry.value(QStringLiteral("file")).toString();
                Macro *macro = macrosByFile.value(file, nullptr);
                if (index >= 0 && index < MaxBuiltinMacros && macro)
                    m_builtins[index] = macro;
            }

            // Load custom shortcuts (version >= 2)
            if (version >= 2) {
                const QJsonObject shortcutsObj = root.value(QStringLiteral("customShortcuts")).toObject();
                for (auto it = shortcutsObj.constBegin(); it != shortcutsObj.constEnd(); ++it) {
                    const QString file = it.key();
                    const QKeySequence shortcut = QKeySequence::fromString(it.value().toString(), QKeySequence::PortableText);
                    Macro *macro = macrosByFile.value(file, nullptr);
                    if (macro && !shortcut.isEmpty()) {
                        m_customShortcuts[macro] = shortcut;
                        macro->setShortcut(shortcut);
                    }
                }
            }
        }
    }

    emit macroListChanged();
}

bool MacroManager::saveToDirectory(const QString &dir) const
{
    QDir directory(dir);
    if (!directory.exists() && !QDir().mkpath(dir))
        return false;

    QHash<Macro *, QString> fileNames;
    QSet<QString> writtenFiles;
    bool allSaved = true;

    for (int i = 0; i < m_userMacros.size(); ++i) {
        Macro *macro = m_userMacros.at(i);
        const QString fileName = QStringLiteral("macro_%1.tpm")
                                     .arg(i + 1, 3, 10, QLatin1Char('0'));
        fileNames.insert(macro, fileName);
        QString error;
        if (macro->saveToFile(directory.filePath(fileName), &error))
            writtenFiles.insert(fileName);
        else
            allSaved = false;
    }

    QJsonArray slotEntries;
    for (int i = 0; i < MaxBuiltinMacros; ++i) {
        Macro *macro = m_builtins[i];
        if (!macro || !fileNames.contains(macro))
            continue;
        QJsonObject entry;
        entry.insert(QStringLiteral("slot"), i);
        entry.insert(QStringLiteral("file"), fileNames.value(macro));
        slotEntries.append(entry);
    }

    QJsonObject customShortcutsObj;
    for (auto it = m_customShortcuts.constBegin(); it != m_customShortcuts.constEnd(); ++it) {
        if (fileNames.contains(it.key()))
            customShortcutsObj[fileNames.value(it.key())] = it.value().toString(QKeySequence::PortableText);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("slots"), slotEntries);
    root.insert(QStringLiteral("customShortcuts"), customShortcutsObj);

    QFile manifest(directory.filePath(QStringLiteral("slots.json")));
    bool manifestSaved = false;
    if (manifest.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        manifestSaved = manifest.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0;
    }

    // Remove macro files left behind by renamed or deleted macros. Do this only
    // when every current macro was written successfully.
    if (allSaved) {
        const QStringList existing = directory.entryList({"*.tpm"}, QDir::Files, QDir::Name);
        for (const QString &file : existing) {
            if (!writtenFiles.contains(file))
                directory.remove(file);
        }
    }

    return allSaved && manifestSaved;
}

QString MacroManager::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/macros");
}

// ---------------------------------------------------------------------------
// Custom shortcut management
// ---------------------------------------------------------------------------

QKeySequence MacroManager::shortcutForMacro(Macro *macro) const
{
    return m_customShortcuts.value(macro, QKeySequence());
}

void MacroManager::setShortcutForMacro(Macro *macro, const QKeySequence &shortcut)
{
    if (!macro)
        return;

    // Verify macro belongs to this manager (built-in or user macro)
    bool owned = false;
    for (int i = 0; i < MaxBuiltinMacros; ++i) {
        if (m_builtins[i] == macro) {
            owned = true;
            break;
        }
    }
    if (!owned && !m_userMacros.contains(macro))
        return;

    if (shortcut.isEmpty()) {
        // Remove shortcut
        if (m_customShortcuts.remove(macro)) {
            macro->setShortcut(QKeySequence());
            emit shortcutChanged(macro, QKeySequence());
            emit macroListChanged();
        }
        return;
    }

    // Check for conflicts (excluding this macro)
    if (!isShortcutAvailable(shortcut, macro))
        return;

    m_customShortcuts[macro] = shortcut;
    macro->setShortcut(shortcut);
    emit shortcutChanged(macro, shortcut);
    emit macroListChanged();
}

void MacroManager::removeShortcutForMacro(Macro *macro)
{
    setShortcutForMacro(macro, QKeySequence());
}

QMap<Macro *, QKeySequence> MacroManager::allCustomShortcuts() const
{
    return m_customShortcuts;
}

static bool isReservedEditorShortcut(const QKeySequence &shortcut)
{
    // Hardcoded list of reserved editor shortcuts
    static const QSet<QString> reserved = {
        // File
        "Ctrl+N", "Ctrl+O", "Ctrl+S", "Ctrl+Shift+S", "Ctrl+P", "Ctrl+W", "Ctrl+Q",
        // Edit
        "Ctrl+Z", "Ctrl+Y", "Ctrl+Shift+Z", "Ctrl+X", "Ctrl+C", "Ctrl+V", "Ctrl+Shift+V",
        "Ctrl+A", "Ctrl+F", "Ctrl+H", "F3", "Shift+F3",
        // View
        "Ctrl++", "Ctrl+-", "Ctrl+0", "F11",
        // Navigation
        "Ctrl+G", "Ctrl+L", "Home", "End", "PageUp", "PageDown",
        "Ctrl+Home", "Ctrl+End", "Alt+Left", "Alt+Right",
        // Selection
        "Shift+Left", "Shift+Right", "Shift+Up", "Shift+Down",
        "Ctrl+Shift+Left", "Ctrl+Shift+Right", "Shift+Home", "Shift+End",
        "Ctrl+Shift+Home", "Ctrl+Shift+End",
        // Window/Tab
        "Ctrl+Tab", "Ctrl+Shift+Tab", "Ctrl+1", "Ctrl+2", "Ctrl+3", "Ctrl+4",
        "Ctrl+5", "Ctrl+6", "Ctrl+7", "Ctrl+8", "Ctrl+9",
        // Tools
        "F5", "F9", "F10", "F11", "F12",
        // Help
        "F1"
    };

    return reserved.contains(shortcut.toString(QKeySequence::PortableText));
}

bool MacroManager::isShortcutAvailable(const QKeySequence &shortcut, Macro *exclude) const
{
    if (shortcut.isEmpty())
        return true;

    const QString shortcutStr = shortcut.toString(QKeySequence::PortableText);

    // Check built-in slots (Ctrl+Shift+1..9)
    for (int i = 0; i < MaxBuiltinMacros; ++i) {
        QKeySequence builtin(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_1 + i);
        if (builtin == shortcut)
            return false;
    }

    // Check other custom shortcuts
    for (auto it = m_customShortcuts.constBegin(); it != m_customShortcuts.constEnd(); ++it) {
        if (it.key() == exclude)
            continue;
        if (it.value() == shortcut)
            return false;
    }

    // Check reserved editor shortcuts
    if (isReservedEditorShortcut(shortcut))
        return false;

    return true;
}

QString MacroManager::shortcutConflictDescription(const QKeySequence &shortcut) const
{
    if (shortcut.isEmpty())
        return QString();

    const QString shortcutStr = shortcut.toString(QKeySequence::PortableText);

    // Check built-in slots
    for (int i = 0; i < MaxBuiltinMacros; ++i) {
        QKeySequence builtin(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_1 + i);
        if (builtin == shortcut) {
            Macro *m = m_builtins[i];
            if (m)
                return tr("Built-in slot %1 (Macro%2)").arg(i + 1).arg(i + 1, 2, 10, QLatin1Char('0'));
            return tr("Built-in slot %1 (Ctrl+Shift+%2)").arg(i + 1).arg(i + 1);
        }
    }

    // Check other custom shortcuts
    for (auto it = m_customShortcuts.constBegin(); it != m_customShortcuts.constEnd(); ++it) {
        if (it.value() == shortcut) {
            return tr("Macro \"%1\"").arg(it.key()->name());
        }
    }

    // Check reserved editor shortcuts
    if (isReservedEditorShortcut(shortcut)) {
        return tr("Reserved editor shortcut");
    }

    return QString();
}

} // namespace papiro