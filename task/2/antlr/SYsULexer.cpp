#include "SYsULexer.hpp"
#include "SYsULexer.tokens.hpp"
#include <unordered_map>
#include <vector>

using antlr4::ParseCancellationException;
using std::make_pair;

namespace {

  using namespace SYsULexerTokens;

  static const std::unordered_map<std::string, size_t> kClangTokens{
    { "eof", antlr4::Token::EOF },
    { "int", kInt },
    { "identifier", kIdentifier },
    { "l_paren", kLeftParen },
    { "r_paren", kRightParen },
    { "return", kReturn },
    { "r_brace", kRightBrace },
    { "l_brace", kLeftBrace },
    { "numeric_constant", kConstant },
    { "semi", kSemi },
    { "equal", kEqual },
    { "plus", kPlus },
    { "minus", kMinus },
    { "comma", kComma },
    { "l_square", kLeftBracket },
    { "r_square", kRightBracket },
    { "star", kStar },
    { "slash", kDiv },
    { "percent", kMod },
    { "amp", kAnd },
    { "pipe", kOr },
    { "caret", kCaret },
    { "tilde", kTilde },
    { "exclaim", kExclaim },
    { "less", kLess },
    { "greater", kGreater },
    { "lessequal", kLessEqual },
    { "greaterequal", kGreaterEqual },
    { "equalequal", kEqualEqual }, //原来是equal_equal,这个要和answer.txt对应
    { "exclaimequal", kExclaimEqual },
    { "ampamp",kAndAnd  },
    { "pipepipe",kOrOr  },
    { "plusplus", kPlusPlus },
    { "minusminus", kMinusMinus },
    { "arrow", kArrow },
    { "dot", kDot },
    { "colon", kColon },
    { "coloncolon", kColonColon },
    { "question", kQuestion },
    { "left_shift", kLeftShift },
    { "right_shift", kRightShift },
    { "ellipsis", kEllipsis },
    { "hash", kHash },
    { "hashhash", kHashHash },
    { "alignas", kAlignas },
    { "alignof", kAlignof },
    { "asm", kAsm },
    { "auto", kAuto },
    { "bool", kBool },
    { "break", kBreak },
    { "case", kCase },
    { "catch", kCatch },
    { "char", kChar },
    { "class", kClass },
    { "const", kConst },
    { "continue", kContinue },
    { "default", kDefault },
    { "delete", kDelete },
    { "do", kDo },
    { "double", kDouble },
    { "else", kElse },
    { "enum", kEnum },
    { "explicit", kExplicit },
    { "export", kExport },
    { "extern", kExtern },
    { "false", kFalse },
    { "float", kFloat },
    { "for", kFor },
    { "friend", kFriend },
    { "goto", kGoto },
    { "if", kIf },
    { "inline", kInline },
    { "long", kLong },
    { "mutable", kMutable },
    { "namespace", kNamespace },
    { "new", kNew },
    { "noexcept", kNoexcept },
    { "nullptr", kNullptr },
    { "operator", kOperator },
    { "override", kOverride },
    { "private", kPrivate },
    { "protected", kProtected },
    { "public", kPublic },
    { "register", kRegister },
    { "reinterpret_cast", kReinterpretCast },
    { "short", kShort },
    { "signed", kSigned },
    { "sizeof", kSizeof },
    { "static", kStatic },
    { "struct", kStruct },
    { "switch", kSwitch },
    { "template", kTemplate },
    { "this", kThis },
    { "thread_local", kThreadLocal },
    { "throw", kThrow },
    { "true", kTrue },
    { "try", kTry },
    { "typedef", kTypedef },
    { "typeid", kTypeid },
    { "typename", kTypename },
    { "union", kUnion },
    { "unsigned", kUnsigned },
    { "using", kUsing },
    { "virtual", kVirtual },
    { "void", kVoid },
    { "volatile", kVolatile },
    { "while", kWhile }
  };


} // namespace

SYsULexer::SYsULexer(antlr4::CharStream* input)
  : mInput(input)
  , mSource(make_pair(this, input))
  , mFactory(antlr4::CommonTokenFactory::DEFAULT.get())
  , mSourceName(input->getSourceName())
{
}

std::unique_ptr<antlr4::Token>
SYsULexer::nextToken()
{
  auto c = mInput->LA(1);
  if (c == antlr4::Token::EOF) {
    // 到达文件末尾，退出循环
    return common_token(antlr4::Token::EOF, mInput->index(), mInput->index());
  }

  auto start = mInput->index();
  std::string line;
  while (true) {
    mInput->consume();
    if (c == U'\n') {
      if (mInput->LA(1) == U'\r')
        mInput->consume();
      break;
    }
    if (c == U'\r') {
      if (mInput->LA(1) == U'\n')
        mInput->consume();
      break;
    }
    line.push_back(c);
    c = mInput->LA(1);
    if (c == antlr4::Token::EOF)
      return common_token(antlr4::Token::INVALID_TYPE, start, mInput->index());
  }
  auto stop = mInput->index();

  std::size_t typeEnd, type;
  std::size_t textEnd;
  std::string text;

  // 提取类型段
  {
    decltype(kClangTokens)::const_iterator typeIter;
    typeEnd = line.find(' ');
    if (typeEnd == std::string::npos)
      goto FAIL;
    typeIter = kClangTokens.find(line.substr(0, typeEnd));
    if (typeIter == kClangTokens.end())
      goto FAIL;
    type = typeIter->second;
  }

  // 提取文本段
  {
    textEnd = line.find('\t', typeEnd + 1);
    if (textEnd == std::string::npos ||
        textEnd < typeEnd + 3 // 至少要有一对空引号 ''
    )
      goto FAIL;
    text = line.substr(typeEnd + 2, textEnd - typeEnd - 3);
  }

  // 提取位置段
  {
    std::size_t locStart, locEnd, colStart, rowStart;

    locStart = line.find("Loc=<", textEnd + 1);
    if (locStart == std::string::npos)
      goto FAIL;
    locEnd = line.rfind(">");
    if (locEnd == std::string::npos)
      goto FAIL;
    colStart = line.rfind(':', locEnd);
    if (colStart == std::string::npos)
      goto FAIL;
    rowStart = line.rfind(':', colStart - 1);
    if (rowStart == std::string::npos)
      goto FAIL;

    mSourceName = line.substr(locStart + 5, rowStart - locStart - 5);
    mLine = std::stoul(line.substr(rowStart + 1, colStart - rowStart - 1));
    mColumn = std::stoul(line.substr(colStart + 1, locEnd - colStart - 1));
  }

  // 解析成功
  return common_token(type, start, stop, text);

FAIL: // 解析失败
  assert(false);
}

size_t
SYsULexer::getLine() const
{
  return mLine;
}

size_t
SYsULexer::getCharPositionInLine()
{
  return mColumn;
}

antlr4::CharStream*
SYsULexer::getInputStream()
{
  return mInput;
}

std::string
SYsULexer::getSourceName()
{
  return mSourceName;
}

antlr4::TokenFactory<antlr4::CommonToken>*
SYsULexer::getTokenFactory()
{
  return mFactory;
}
