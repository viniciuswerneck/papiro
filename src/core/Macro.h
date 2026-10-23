#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>

class QPlainTextEdit;  // forward declaration

namespace papiro {

/*! A single recorded macro action. */
struct MacroAction {
    enum Type {
        TextInput,          // plain text typed
        KeyPress,           // special key (Enter, Tab, Backspace, arrows, etc.)
        EditorCommand,      // high-level command (cut, copy, paste, undo, etc.)
        CursorMovement      // explicit cursor move (rare, derived from keys)
    } type = TextInput;

    QString text;           // for TextInput: the inserted text
    int key = 0;            // for KeyPress: Qt::Key
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    QString command;        // for EditorCommand: action name
    int repeat = 1;         // repeat count (for optimized playback)

    static MacroAction fromTextInput(const QString &text);
    static MacroAction fromKeyPress(int key, Qt::KeyboardModifiers modifiers);
    static MacroAction fromCommand(const QString &command);
};

/*! A complete macro: sequence of actions with metadata. */
class Macro : public QObject {
    Q_OBJECT

public:
    explicit Macro(QObject *parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    QString description() const { return m_description; }
    void setDescription(const QString &desc) { m_description = desc; }

    QDateTime created() const { return m_created; }
    QDateTime modified() const { return m_modified; }

    const QVector<MacroAction> &actions() const { return m_actions; }
    QVector<MacroAction> &actions() { return m_actions; }

    void addAction(const MacroAction &action);
    void clear();

    int estimatedDurationMs() const;  // rough playback time estimate

    bool saveToFile(const QString &path, QString *error = nullptr) const;
    static Macro *loadFromFile(const QString &path, QString *error = nullptr, QObject *parent = nullptr);

    /*! Create a macro from a .TPM-like text format (for compatibility/debug). */
    static Macro *fromTextFormat(const QString &text, QString *error = nullptr, QObject *parent = nullptr);
    QString toTextFormat() const;

private:
    QString m_name;
    QString m_description;
    QDateTime m_created;
    QDateTime m_modified;
    QVector<MacroAction> m_actions;
};

/*! Central manager: owns built-in macros (Macro01..NN) and user macros. */
class MacroManager : public QObject {
    Q_OBJECT

public:
    explicit MacroManager(QObject *parent = nullptr);

    /*! Built-in slots: Macro01 (Ctrl+Shift+1) ... MacroNN (Ctrl+Shift+9). */
    static constexpr int MaxBuiltinMacros = 9;

    /*! Macro currently being recorded, or nullptr. */
    Macro *recordingMacro() const { return m_recording; }

    /*! Start recording a new macro. */
    void startRecording(const QString &name = QString());

    /*! Stop recording; returns the finished macro (owned by manager). */
    Macro *stopRecording();

    /*! Cancel recording without saving. */
    void cancelRecording();

    /*! Play a macro on the given editor. */
    void playMacro(Macro *macro, QPlainTextEdit *editor, int repeatCount = 1);

    /*! Play built-in macro by index (0 = Macro01). */
    void playBuiltin(int index, QPlainTextEdit *editor, int repeatCount = 1);

    /*! Assign a macro to a built-in slot (takes ownership). */
    void setBuiltinMacro(int index, Macro *macro);

    /*! Get built-in macro (may be nullptr). */
    Macro *builtinMacro(int index) const;

    /*! All user macros (excluding built-in slots). */
    const QVector<Macro *> &userMacros() const { return m_userMacros; }

    /*! Load all macros from the user macros directory. */
    void loadFromDirectory(const QString &dir);

    /*! Save all user macros to the user macros directory. */
    void saveToDirectory(const QString &dir) const;

    /*! Default user macros directory. */
    static QString defaultDirectory();

signals:
    void recordingStarted(Macro *macro);
    void recordingStopped(Macro *macro);
    void macroListChanged();

private:
    Macro *m_recording = nullptr;
    Macro *m_builtins[MaxBuiltinMacros] = {nullptr};
    QVector<Macro *> m_userMacros;
};

/*! Helper: execute a single MacroAction on an Editor (implemented in Editor.cpp). */
bool executeMacroAction(const MacroAction &action, QPlainTextEdit *editor);

} // namespace papiro