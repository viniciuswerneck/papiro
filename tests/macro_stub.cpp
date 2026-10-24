#include "core/Macro.h"

#include <QPlainTextEdit>

namespace papiro {

// Unit-test binaries that link papiro_core but not the editor still pull
// Macro.cpp in through the moc compilation unit. Macro playback needs the
// real editor implementation, so provide a minimal stub here.
bool executeMacroAction(const MacroAction &, QPlainTextEdit *)
{
    return false;
}

} // namespace papiro
