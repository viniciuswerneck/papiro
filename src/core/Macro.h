#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QKeySequence>
#include <QMap>

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
    Q_PROPERTY(QKeySequence shortcut READ shortcut WRITE setShortcut NOTIFY shortcutChanged)

public:
    explicit Macro(QObject *parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    QString description() const { return m_description; }
    void setDescription(const QString &desc) { m_description = desc; }

    QDateTime created() const { return m_created; }
    QDateTime modified() const { return m_modified; }

    QKeySequence shortcut() const { return m_shortcut; }
    void setShortcut(const QKeySequence &shortcut);

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

signals:
    void shortcutChanged(const QKeySequence &shortcut);

private:
    QString m_name;
    QString m_description;
    QDateTime m_created;
    QDateTime m_modified;
    QKeySequence m_shortcut;
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

    /*! Assign an existing macro to a built-in slot (manager retains ownership).
     *  Passing nullptr clears the slot without deleting the macro. */
    void setBuiltinMacro(int index, Macro *macro);

    /*! Clear a built-in slot without deleting its macro. */
    void clearBuiltinMacro(int index);

    /*! Get built-in macro (may be nullptr). */
    Macro *builtinMacro(int index) const;

    /*! Remove a user macro, clearing any slots that referenced it. */
    bool removeUserMacro(Macro *macro);

    /*! All user macros (excluding built-in slots). */
    const QVector<Macro *> &userMacros() const { return m_userMacros; }

    /*! Get the custom shortcut assigned to a macro, or empty if none. */
    QKeySequence shortcutForMacro(Macro *macro) const;

    /*! Assign a custom shortcut to a macro. Validates ownership and conflicts. */
    void setShortcutForMacro(Macro *macro, const QKeySequence &shortcut);

    /*! Remove any custom shortcut assigned to a macro. */
    void removeShortcutForMacro(Macro *macro);

    /*! Return all custom shortcut mappings (copy). */
    QMap<Macro *, QKeySequence> allCustomShortcuts() const;

    /*! Check if a shortcut is available (not used by built-ins, other macros, or reserved editor shortcuts). */
    bool isShortcutAvailable(const QKeySequence &shortcut, Macro *exclude = nullptr) const;

    /*! Get a human-readable description of what a shortcut conflicts with. */
    QString shortcutConflictDescription(const QKeySequence &shortcut) const;

    /*! Load all macros from the user macros directory. */
    void loadFromDirectory(const QString &dir);

    /*! Save all user macros and shortcut slots to the macros directory. */
    bool saveToDirectory(const QString &dir) const;

    /*! Default user macros directory. */
    static QString defaultDirectory();

signals:
    void recordingStarted(Macro *macro);
    void recordingStopped(Macro *macro);
    void macroListChanged();
    void shortcutChanged(Macro *macro, const QKeySequence &newShortcut);

private:
    Macro *m_recording = nullptr;
    Macro *m_builtins[MaxBuiltinMacros] = {nullptr};
    QVector<Macro *> m_userMacros;
    QMap<Macro *, QKeySequence> m_customShortcuts;
};

/*! Helper: execute a single MacroAction on an Editor (implemented in Editor.cpp). */
bool executeMacroAction(const MacroAction &action, QPlainTextEdit *editor);

} // namespace papiro