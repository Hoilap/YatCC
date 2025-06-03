lexer grammar SYsULexer;

@lexer::members {
    public:
        // 用于跟踪预处理行的数量
        int preprocessingLines_1 = 0;
        int preprocessingLines_2 = 0;
        // 用于存储提取的文件路径
        std::string filePath_1 = "";
        std::string filePath_2 = "";
        // 用于跟踪自定义行的数量
        int customLine = 1;
        // 用于判断是否遇到代码行
        bool flag_meet_code = false;
        // 用于判断是否完成第一个文件的处理
        bool flag_finish_first = false;
        // 用于存储行号变化的位置
        int line_change_code = INT_MAX;

        // 提取文件路径
        void extract_path(const std::string& line) {
            size_t start = line.find("\"");
            if (start != std::string::npos) {
                size_t end = line.find("\"", start + 1);
                if (end != std::string::npos) {
                    std::string filePath = line.substr(start + 1, end - start - 1);
                    if (!flag_finish_first) {
                        filePath_1 = filePath;
                    } else {
                        filePath_2 = filePath;
                    }
                }
            }
        }

        // 提取行号
        void extract_pre_num(const std::string& line) {
            size_t lineNumberStart = line.find_first_of("0123456789");
            if (lineNumberStart != std::string::npos) {
                size_t lineNumberEnd = line.find_first_not_of("0123456789", lineNumberStart);
                if (lineNumberEnd != std::string::npos) {
                    std::string lineNumberStr = line.substr(lineNumberStart, lineNumberEnd - lineNumberStart);
                    int num = std::stoi(lineNumberStr);
                    std::cout << "num: " << num <<std::endl;
                    if (!flag_finish_first) {
                        preprocessingLines_1 = customLine-num+1;
                        std::cout << customLine<<"  "<<num<<"  " << preprocessingLines_1 <<std::endl;
                    } else {
                        preprocessingLines_2 = customLine-num+1;
                        std::cout << customLine<<"  "<<num<<"  " << preprocessingLines_2 <<std::endl;
                    }
                }
            }
        }

        // 提取文件路径和行号
        void extract(const std::string& line) {
            extract_pre_num(line);
            extract_path(line);
        }

        // 检测是否遇到普通代码行
        // void checkIfCodeLine(const std::string& line) {
        //     if (!line.empty() && line[0] != '#') {
        //         flag_meet_code = true;
        //     }
        // }
}

// 保留原有token定义
Int : 'int';
Void : 'void';
Char : 'char';
Float : 'float';
Double : 'double';
Short : 'short';
Long : 'long';
Const : 'const';
Return : 'return';

LeftParen : '(';
RightParen : ')';
LeftBracket : '[';
RightBracket : ']';
LeftBrace : '{';
RightBrace : '}';

Plus : '+';
Minus : '-';
Star : '*';
Slash : '/';
Percent : '%';

Semi : ';' {
    flag_meet_code = true; // 遇到分号时设置 flag_meet_code = true
};
Comma : ',';

Equal : '=';
EqualEqual : '==';
NotEqual : '!=';
Less : '<';
Greater : '>';
LessEqual : '<=';
GreaterEqual : '>=';

AndAnd : '&&';
OrOr : '||';
Not : '!';

If : 'if';
Else : 'else';
While : 'while';
For : 'for';
Continue :'continue';
Break : 'break';

Identifier
    :   IdentifierNondigit
        (   IdentifierNondigit
        |   Digit
        )* 
    ;

fragment
IdentifierNondigit
    :   Nondigit
    ;

fragment
Nondigit
    :   [a-zA-Z_]
    ;

fragment
Digit
    :   [0-9]
    ;

Constant
    :   IntegerConstant
    |   FloatingConstant ;

fragment
IntegerConstant
    :   DecimalConstant
    |   OctalConstant
    |   HexadecimalConstant
    ;

fragment
DecimalConstant
    :   NonzeroDigit Digit*
    ;

fragment
OctalConstant
    :   '0' OctalDigit*
    ;

fragment
HexadecimalConstant
    :   HexadecimalPrefix HexadecimalDigit+
    ;

fragment
HexadecimalPrefix
    :   '0' [xX]
    ;

fragment
NonzeroDigit
    :   [1-9]
    ;

fragment
OctalDigit
    :   [0-7]
    ;

fragment
HexadecimalDigit
    :   [0-9a-fA-F]
    ;

<<<<<<< Updated upstream
// 预处理信息处理，可以从预处理信息中获得文件名以及行号
// 预处理信息中的第一个数字即为行号
=======
fragment
FloatingConstant
    :   DecimalFloatingConstant
    |   HexadecimalFloatingConstant
    ;

fragment
DecimalFloatingConstant
    :   FractionalConstant ExponentPart?
    |   DigitSequence ExponentPart
    ;

fragment
HexadecimalFloatingConstant
    :   HexadecimalPrefix HexadecimalFractionalConstant BinaryExponentPart
    ;

fragment
FractionalConstant
    :   DigitSequence? '.' DigitSequence
    |   DigitSequence '.'
    ;

fragment
HexadecimalFractionalConstant
    :   HexadecimalDigitSequence? '.' HexadecimalDigitSequence
    |   HexadecimalDigitSequence '.'
    ;

fragment
ExponentPart
    :   [eE] Sign? DigitSequence
    ;

fragment
BinaryExponentPart
    :   [pP] Sign? DigitSequence
    ;

fragment
Sign
    :   [+-]
    ;

fragment
DigitSequence
    :   Digit+
    ;

fragment
HexadecimalDigitSequence
    :   HexadecimalDigit+
    ;

StringLiteral
    :   '"' (EscapeSequence | ~["\\\r\n])* '"'
    ;

fragment
EscapeSequence
    :   '\\' [abfnrtv'"\\]
    |   '\\' OctalDigit OctalDigit? OctalDigit?
    |   '\\x' HexadecimalDigit+
    ;

// 处理预处理指令，提取文件路径和行号
>>>>>>> Stashed changes
LineAfterPreprocessing
    : '#' ~[\r\n]* {
        if (flag_meet_code) {
            flag_finish_first = true;
            line_change_code = customLine;
        }
        extract(getText());
    } -> skip
    ;

// 处理换行符
Newline
    :   ( '\r' '\n'?
        | '\n'
        )
        {
            customLine++;
            // 检查当前行是否是普通代码行
            //checkIfCodeLine( getText());
        }
        //-> skip
    ;

Whitespace
    :   [ \t]+
    ;

BlockComment
    :   '/*' .*? '*/'
        -> skip
    ;

LineComment
    :   '//' ~[\r\n]* {
    } -> skip
    ;
