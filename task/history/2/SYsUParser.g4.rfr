parser grammar SYsUParser;

options {
  tokenVocab=SYsULexer;
}

arrayExpression
    :   arrayExpression LeftBracket assignmentExpression RightBracket
    |   Identifier
    ;

argumentList
    :   assignmentExpression (Comma assignmentExpression)*
    ;

funcCaller
    :   Identifier LeftParen argumentList? RightParen
    ;

primaryExpression
    :   arrayExpression
    |   Constant
    |   funcCaller
    ;

postfixExpression
    :   primaryExpression  
    ;

unaryExpression
    :
    (postfixExpression
    |   unaryOperator parenExpression
    )
    ;

unaryOperator
    :   Plus | Minus | Exclaim
    ;

parenExpression
    :   LeftParen additiveExpression RightParen
    |   unaryExpression
    ;

multiplicativeExpression
    :   multiplicativeExpression (Star|Slash|Percent) parenExpression
    |   parenExpression
    ;

additiveExpression
    :   additiveExpression (Plus|Minus) multiplicativeExpression
    |   multiplicativeExpression
    ;


conditionComparationExpression
    :   conditionComparationExpression (Less|Lessequal|Greater|Greaterequal) additiveExpression
    |   additiveExpression
    ;

conditionEqualityExpression
    :   conditionEqualityExpression (Exclaimequal|Equalequal) conditionComparationExpression
    |   conditionComparationExpression
    ;

logicOrExpression
    :   logicOrExpression Pipepipe logicAndExpression
    |   logicAndExpression
    ;

logicAndExpression
    :   logicAndExpression Ampamp conditionEqualityExpression
    |   conditionEqualityExpression
    ;

assignmentExpression
    :   logicOrExpression
    // :   additiveExpression
    |   unaryExpression Equal assignmentExpression
    ;

expression
    :   assignmentExpression (Comma assignmentExpression)*
    ;


declaration
    :   declarationSpecifiers initDeclaratorList? Semi
    ;

declarationSpecifiers
    :   declarationSpecifier+
    ;

declarationSpecifier
    :   typeSpecifier
    ;

initDeclaratorList
    :   initDeclarator (Comma initDeclarator)*
    ;

initDeclarator
    :   declarator (Equal initializer)?
    ;


typeSpecifier
    :   Int
    |   Const Int
    |   Void
    ;


declarator
    :   directDeclarator
    ;

directDeclarator
    :   Identifier
    |   directDeclarator LeftBracket assignmentExpression? RightBracket
    ;

identifierList
    :   Identifier (Comma Identifier)*
    ;

initializer
    :   assignmentExpression
    |   LeftBrace initializerList? Comma? RightBrace
    ;

initializerList
    // :   designation? initializer (Comma designation? initializer)*
    :   initializer (Comma initializer)*
    ;


ifStatement
    :   If LeftParen assignmentExpression RightParen statement (Else statement)?
    ;

whileStatement
    :   While LeftParen assignmentExpression RightParen statement
    ;

statement
    :   compoundStatement
    |   expressionStatement
    |   jumpStatement
    |   ifStatement
    |   whileStatement
    |   breakStatement
    |   continueStatement
    ;

compoundStatement
    :   LeftBrace blockItemList? RightBrace
    ;

blockItemList
    :   blockItem+
    ;

blockItem
    :   statement
    |   declaration
    ;

expressionStatement
    :   expression? Semi
    ;

breakStatement
    :   Break Semi;

continueStatement
    :   Continue Semi;


jumpStatement
    :   (Return expression?) Semi
    ;

compilationUnit
    :   translationUnit? EOF
    ;

translationUnit
    :   externalDeclaration+
    ;

externalDeclaration
    :   functionDefinition
    |   declaration
    ;

funcDeclaration
    :   declarationSpecifiers initDeclarator (Comma declarationSpecifiers initDeclarator)*
    ;

functionDefinition
    : declarationSpecifiers directDeclarator LeftParen funcDeclaration? RightParen (compoundStatement|Semi)
    ;

