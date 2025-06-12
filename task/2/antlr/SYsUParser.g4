parser grammar SYsUParser;

options {
  tokenVocab=SYsULexer;
}

// 表达式部分
expression
    : assignmentExpression (Comma assignmentExpression)* // 🔥 支持逗号表达式
    ;

assignmentExpression
    : logicalOrExpression 
    | unaryExpression Equal assignmentExpression // 🔥 修改：赋值是右结合，允许 a = b = c
    ;

logicalOrExpression
    : logicalAndExpression (OrOr logicalAndExpression)* // 🔥 新增，支持 ||
    ;

logicalAndExpression
    : equalityExpression (AndAnd equalityExpression)* // 🔥 新增，支持 &&
    ;

equalityExpression
    : relationalExpression ( (EqualEqual | ExclaimEqual) relationalExpression )* // 🔥 新增，支持 == !=
    ;

relationalExpression
    : additiveExpression ( (Less | Greater | LessEqual | GreaterEqual) additiveExpression )* // 🔥 新增，支持 < > <= >=
    ;

additiveExpression
    : multiplicativeExpression ( (Plus | Minus) multiplicativeExpression )* // 🔥 改动：加法在乘法之后
    ;

multiplicativeExpression
    : unaryExpression ( (Star | Div | Mod) unaryExpression )* // 🔥 新增，支持乘除模运算
    ;

unaryExpression
    : postfixExpression
    | unaryOperator unaryExpression
    ;

unaryOperator
    : Plus
    | Minus
    | Exclaim
    ;

postfixExpression
    : primaryExpression
    | postfixExpression LeftBracket expression RightBracket  // 🔥 新增，支持数组访问 a[i]
    | postfixExpression LeftParen argumentExpressionList? RightParen // 🔥 新增，支持函数调用 f(a,b)
    ;
    
primaryExpression
    : Identifier
    | Constant
    | LeftParen expression RightParen        // 🔥 新增，支持括号表达式 (a+b) √
    ;

argumentExpressionList
    : assignmentExpression (Comma assignmentExpression)*
    ;











// 声明与定义
declaration //int// a=5,b=4//;
    : declarationSpecifiers initDeclaratorList? Semi
    ;

declarationSpecifiers
    : declarationSpecifier+
    ;

declarationSpecifier
    : typeSpecifier
    ;

typeSpecifier
    : Int // 目前仅支持 int 类型，可以扩展
    | Const
    | Void
    ;

initDeclaratorList// 多个初始化声明 a=5,b=4;
    : initDeclarator (Comma initDeclarator)*
    ;

initDeclarator //
    : declarator (Equal initializer)?
    ;

declarator
    : directDeclarator
    ;

directDeclarator
    : Identifier
    | directDeclarator LeftBracket assignmentExpression? RightBracket // 支持数组声明 a[10]; 多维数组也能递归
    ;

initializer
    : assignmentExpression
    | LeftBrace initializerList (Comma)? RightBrace // 🔥 支持数组初始化 int a[3]={1,2,3}
    ;

initializerList
    : initializer (Comma initializer)*
    ;

// 语句
statement
    : compoundStatement
    | expressionStatement
    | selectionStatement // 🔥 新增 if else 语句
    | iterationStatement // 🔥 新增 while 循环
    | jumpStatement
    ;

compoundStatement
    : LeftBrace blockItemList? RightBrace
    ;

blockItemList
    : blockItem+
    ;

blockItem
    : declaration
    | statement
    ;

expressionStatement
    : expression? Semi
    ;

selectionStatement
    : If LeftParen expression RightParen statement (Else statement)? // 🔥 支持 if-else
    ;

iterationStatement
    : While LeftParen expression RightParen statement // 🔥 支持 while 循环
    ;

jumpStatement
    : Return expression? Semi
    | Continue Semi // 🔥 新增，支持 continue
    | Break Semi // 🔥 新增，支持 break
    ;

// 函数定义与编译单元
functionDefinition
    : declarationSpecifiers directDeclarator LeftParen parameterList? RightParen (compoundStatement| Semi) // 🔥 修改：函数定义必须是 compoundStatement，不是单独 ;
    ;

parameterList//int a,int b
    : parameterDeclaration (Comma parameterDeclaration)*
    ;

parameterDeclaration//int//a
    : declarationSpecifiers declarator
    ;

externalDeclaration
    : functionDefinition
    | declaration
    ;

translationUnit
    : externalDeclaration+
    ;

compilationUnit
    : translationUnit? EOF
    ;
