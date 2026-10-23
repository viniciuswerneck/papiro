#include "core/Macro.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
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
    if (m_builtins[index])
        m_builtins[index]->deleteLater();
    m_builtins[index] = macro;
    if (macro)
        macro->setParent(this);
    emit macroListChanged();
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

    const QStringList files = directory.entryList({"*.tpm"}, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        QString error;
        Macro *macro = Macro::loadFromFile(directory.filePath(file), &error, this);
        if (macro) {
            m_userMacros.append(macro);
        }
    }
    emit macroListChanged();
}

void MacroManager::saveToDirectory(const QString &dir) const
{
    QDir directory(dir);
    if (!directory.exists())
        directory.mkpath(".");

    for (int i = 0; i < m_userMacros.size(); ++i) {
        Macro *macro = m_userMacros.at(i);
        QString fileName = QString("macro_%1.tpm").arg(i + 1, 3, 10, QChar('0'));
        if (!macro->name().isEmpty()) {
            QString safeName = macro->name();
            safeName.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
            fileName = safeName + QString(".tpm");
        }
        QString error;
        macro->saveToFile(directory.filePath(fileName), &error);
    }
}

QString MacroManager::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/macros");
}

} // namespace papiro