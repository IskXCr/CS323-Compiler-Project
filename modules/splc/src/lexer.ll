%{

/* Implementation of yyFlexScanner */
#include "core/utils.hh"
#include "scanner.hh"
#undef  YY_DECL
#define YY_DECL int splc::Scanner::yylex(splc::Parser::value_type *const lval, splc::Parser::location_type *loc)

/* Required std headers */
#include <string>
#include <iostream>

/* typedef to make the returns for the tokens shorter */
using Token = splc::Parser::token;

/* define yyterminate as this instead of NULL */
// #define yyterminate() return( Token::END )

/* update location on matching */
#define YY_USER_ACTION loc->step(); loc->columns(yyleng);

%}

/* Option section */
%option prefix="Splc"
%option debug
%option nodefault
%option yyclass="splc::Scanner"
%option noyywrap
%option c++

/* Token section */
%%
%{          /** Code executed at the beginning of yylex **/
            yylval = lval;
            loc->begin.filename = &filename;
            loc->end.filename = &filename;
%}

[a-z]       {
                return( Token::LOWER );
            }

[A-Z]       {
                return( Token::UPPER );
            }

[a-zA-Z]+   {
                *yylval = yytext;
                return( Token::WORD );
            }

\n          {
                // Update line number
                loc->lines();
                return( Token::NEWLINE );
            }

.           {
                SPLC_LOG_ERROR(*loc, "Unrecognized symbol");
            }
%%