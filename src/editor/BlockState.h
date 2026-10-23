#pragma once

namespace papiro {

/*!
 * Bits stored in QTextBlock::userState.
 *
 * QSyntaxHighlighter and the folding code both use the block user state, so
 * they must agree on which bits belong to whom. The low bits hold the syntax
 * state; the folding bit lives above that mask and is preserved whenever the
 * highlighter updates a block.
 */
namespace BlockState {

inline constexpr int SyntaxMask = 0x0FFF;
inline constexpr int Folded = 0x1000;

} // namespace BlockState

} // namespace papiro
