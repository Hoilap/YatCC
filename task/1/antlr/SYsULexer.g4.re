lexer grammar SYsULexer;

@header {
  #include <iostream>
  #include <string>
  #include <map>
}

@members {
  std::string tokenType;
  std::string tokenText;
  std::string sourceName;
  int tokenLine;
  int tokenColumn;

  std::map<std::string, int> kClangTokens = {
    {"EOF", 0}, {"Int", 1}, {"Identifier", 2}, {"LeftParen", 3},
    {"RightParen", 4}, {"Return", 5}, {"RightBrace", 6}, {"LeftBrace", 7},
    {"Constant", 8}, {"Semi", 9}, {"Equal", 10}, {"Plus", 11},
    {"Minus", 12}, {"Comma", 13}, {"LeftBracket", 14}, {"RightBracket", 15},
    {"Star", 16}, {"Slash", 17}, {"Percent", 18}, {"Amp", 19},
    {"Pipe", 20}, {"Caret", 21}, {"Tilde", 22}, {"Exclaim", 23},
    {"Less", 24}, {"Greater", 25}, {"LessEqual", 26}, {"GreaterEqual", 27},
    {"EqualEqual", 28}, {"ExclaimEqual", 29}, {"AmpAmp", 30}, {"PipePipe", 31},
    {"PlusPlus", 32}, {"MinusMinus", 33}, {"Arrow", 34}, {"Dot", 35},
    {"Colon", 36}, {"ColonColon", 37}, {"Question", 38}, {"LeftShift", 39},
    {"RightShift", 40}, {"Ellipsis", 41}, {"Hash", 42}, {"HashHash", 43},
    {"Alignas", 44}, {"Alignof", 45}, {"Asm", 46}, {"Auto", 47},
    {"Bool", 48}, {"Break", 49}, {"Case", 50}, {"Catch", 51},
    {"Char", 52}, {"Class", 53}, {"Const", 54}, {"Continue", 55},
    {"Default", 56}, {"Delete", 57}, {"Do", 58}, {"Double", 59},
    {"Else", 60}, {"Enum", 61}, {"Explicit", 62}, {"Export", 63},
    {"Extern", 64}, {"False", 65}, {"Float", 66}, {"For", 67},
    {"Friend", 68}, {"Goto", 69}, {"If", 70}, {"Inline", 71},
    {"Long", 72}, {"Mutable", 73}, {"Namespace", 74}, {"New", 75},
    {"Noexcept", 76}, {"Nullptr", 77}, {"Operator", 78}, {"Override", 79},
    {"Private", 80}, {"Protected", 81}, {"Public", 82}, {"Register", 83},
    {"ReinterpretCast", 84}, {"Short", 85}, {"Signed", 86}, {"Sizeof", 87},
    {"Static", 88}, {"Struct", 89}, {"Switch", 90}, {"Template", 91},
    {"This", 92}, {"ThreadLocal", 93}, {"Throw", 94}, {"True", 95},
    {"Try", 96}, {"Typedef", 97}, {"Typeid", 98}, {"Typename", 99},
    {"Union", 100}, {"Unsigned", 101}, {"Using", 102}, {"Virtual", 103},
    {"Void", 104}, {"Volatile", 105}, {"While", 106}
  };
}

/** 处理换行符 */
NEWLINE
    : ('\r'? '\n') -> skip
    ;

/** 忽略空白字符 */
WHITESPACE
    : [ \t]+ -> skip
    ;

/** 解析 Token 类型（例如 IDENTIFIER, INT, RETURN） */
TOKEN_TYPE
    : [A-Za-z_][A-Za-z_0-9]*
    {
        auto it = kClangTokens.find(getText());
        if (it != kClangTokens.end()) {
            tokenType = getText();
        } else {
            tokenType = "INVALID_TYPE"; // 未知 Token 类型
        }
    }
    ;

/** 解析 Token 文本（单引号包围的文本，例如 'x' 或 '42'） */
TOKEN_TEXT
    : '\'' (~['\t\r\n] | '\\' .)* '\''
    {
        std::string rawText = getText();
        tokenText = rawText.substr(1, rawText.length() - 2); // 去掉单引号
    }
    ;

/** 解析位置信息（Loc=<filename:line:column>） */
LOCATION
    : 'Loc=<' (~[>\r\n])+ '>'
    {
        std::string locText = getText();
        size_t firstColon = locText.find(':');
        size_t secondColon = locText.find(':', firstColon + 1);
        size_t endBracket = locText.find('>');

        if (firstColon != std::string::npos && secondColon != std::string::npos && endBracket != std::string::npos) {
            sourceName = locText.substr(5, firstColon - 5);
            tokenLine = std::stoi(locText.substr(firstColon + 1, secondColon - firstColon - 1));
            tokenColumn = std::stoi(locText.substr(secondColon + 1, endBracket - secondColon - 1));
        } else {
            sourceName = "UNKNOWN";
            tokenLine = -1;
            tokenColumn = -1;
        }
    }
    ;

/** 解析完整的 Token */
TOKEN
    : TOKEN_TYPE ' ' TOKEN_TEXT '\t' LOCATION
    ;
