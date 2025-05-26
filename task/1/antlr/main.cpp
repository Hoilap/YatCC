#include "SYsULexer.h" // 确保这里的头文件名与您生成的词法分析器匹配
#include <fstream>
#include <iostream>
#include <unordered_map>

bool startofline_flag=0;
bool space_flag=0;
// 映射定义，将ANTLR的tokenTypeName映射到clang的格式
std::unordered_map<std::string, std::string> tokenTypeMapping = {
  { "Int", "int" },
  {"Void","void"},
  {"Float","float"},
  {"Const","const"},
  { "Identifier", "identifier" },
  { "LeftParen", "l_paren" },
  { "RightParen", "r_paren" },
  { "LeftBrace", "l_brace" },
  { "RightBrace", "r_brace" },
  { "LeftBracket", "l_square" },
  { "RightBracket", "r_square" },
  { "Constant", "numeric_constant" },
  { "Return", "return" },
  { "Semi", "semi" },
  { "EOF", "eof" },
  { "Equal", "equal" },
  { "Plus", "plus" },
  { "Minus", "minus" },
  { "Star", "star" },
  { "Slash", "slash" },
  { "Percent", "percent" }, // 添加对 % 的映射
  { "Comma", "comma" },
  { "EqualEqual", "equalequal" },
  { "NotEqual", "exclaimequal" },
  { "Less", "less" },
  { "Greater", "greater" },
  { "LessEqual", "lessequal" },
  { "GreaterEqual", "greaterequal" },
  { "AndAnd", "ampamp" },
  { "OrOr", "pipepipe" },
  { "Not", "exclaim" },
  { "If", "if" },
  { "Else", "else" },
  { "While", "while" },
  { "For", "for" },
  { "StringLiteral", "string_literal" },
  { "LineComment", "line_comment" },
  { "BlockComment", "block_comment" },
  {"Whitespace","whitespace"},
  {"Newline","ewline"},
  { "Continue", "continue" },
  {"Break","break"}
};

void print_token(const antlr4::Token* token,
            const antlr4::CommonTokenStream& tokens,
            //std::string infilePath,
            std::ofstream& outFile,
            antlr4::Lexer& lexer)
            //SYsULexer lexer)
{
  //使用下面这一行代码就相当于把我们在`g4`文件中取的所有的别名都放到了`vocabulary`这个数据结构中。
  auto& vocabulary = lexer.getVocabulary();

  auto tokenTypeName = //获得比如Whitespace这样的名字
    std::string(vocabulary.getSymbolicName(token->getType()));

  if (tokenTypeName.empty())
    tokenTypeName = "<UNKNOWN>"; // 处理可能的空字符串情况

  if (tokenTypeMapping.find(tokenTypeName) != tokenTypeMapping.end()) {
    tokenTypeName = tokenTypeMapping[tokenTypeName];
  }

  //记录换行
  if(tokenTypeName=="ewline"){
    startofline_flag=1;
    return;
  }
  //记录空格
  if (tokenTypeName == "whitespace") {
    space_flag=1;
    return;
  }

  //任务：重写locInfo
  //std::string locInfo = " Loc=<0:0>";
  const SYsULexer* sysuLexer = dynamic_cast<const SYsULexer*>(&lexer);//动态编译
  int real_line=token->getLine();
  int line;
  std::string filepath;

  std::cout<<sysuLexer->line_change_code<<"   "<<sysuLexer->preprocessingLines_1<<"    "<<sysuLexer->filePath_1<<std::endl;
  if(real_line<sysuLexer->line_change_code){
    line=real_line-sysuLexer->preprocessingLines_1;
    filepath=sysuLexer->filePath_1;
  }
  else{
    line=real_line-sysuLexer->preprocessingLines_2;
    filepath=sysuLexer->filePath_2;
  }
  std::string locInfo = " Loc=<" + filepath + ":" +
                                   std::to_string(line) + ":" +
                                   std::to_string(token->getCharPositionInLine() + 1) +
  ">";
  //sysuLexer->filePath + ":" +
  //std::to_string(token->getLine()-sysuLexer->preprocessingLines)  + ":" +
  //outFile << int(lexer.preprocessline) << std::endl;


  //任务：重写地址（从预处理信息中提取，不是从inFile中提取）
  //lexer.getGrammarFileName()

  //任务：重写startOfLine/leadingSpace
  //这个变量通常用于表示当前 token 是否是一行的开始。如果 token 位于一行的开头，startOfLine 应该为 true，否则为 false。
  //注意：换行之后会遇到tab,因此不能直接通过列号来判断
  //bool startOfLine = (token->getCharPositionInLine() == 0);
  bool startOfLine = startofline_flag;
  //leadingSpace检查前一个字符是否是空格
  bool leadingSpace = space_flag;

  if (token->getText() != "<EOF>")//当 token 是文件结束符（EOF）时，不输出 token 的文本内容
    outFile << tokenTypeName << " '" << token->getText() << "'";
  else
    outFile << tokenTypeName << " '"
            << "'";

  if (startOfLine)
    outFile << "\t [StartOfLine]\t";
  if (leadingSpace)
    outFile << "\t [LeadingSpace]\t";
  startofline_flag=0;
  space_flag=0;
  outFile << locInfo << std::endl;
}

int main(int argc, char* argv[])
{
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <input> <output>\n";
    return -1;
  }

  std::ifstream inFile(argv[1]);
  if (!inFile) {
    std::cout << "Error: unable to open input file: " << argv[1] << '\n';
    return -2;
  }
  std::string infilePath = argv[1]; // 保存文件路径

  std::ofstream outFile(argv[2]);
  if (!outFile) {
    std::cout << "Error: unable to open output file: " << argv[2] << '\n';
    return -3;
  }

  std::cout << "程序 '" << argv[0] << std::endl;
  std::cout << "输入 '" << argv[1] << std::endl;
  std::cout << "输出 '" << argv[2] << std::endl;

  antlr4::ANTLRInputStream input(inFile);
  SYsULexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  tokens.fill();

  for (auto&& token : tokens.getTokens()) {
    print_token(token, tokens, outFile, lexer);
  }
}
