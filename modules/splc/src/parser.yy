%skeleton "lalr1.cc"
%require  "3.8.2"
%debug 

%define api.namespace {splc}
%define api.parser.class {Parser}

%code requires{
   namespace splc {
      class Driver;
      class Scanner;
   }
}

%parse-param { Scanner  &scanner  }
%parse-param { Driver  &driver  }

%code{
   #include <iostream>
   #include <cstdlib>
   #include <fstream>
   
   /* include for all driver functions */
   #include "driver.hh"

#undef yylex
#define yylex scanner.yylex
}

%define api.symbol.prefix {} // The empty prefix is generally invalid, but there is namespace in C++.
%define api.value.type { std::string }
%define parse.assert

%locations
%define api.location.file "../include/location.hh"

/* Start of token definition section */
/* %token END 0 */
%token UPPER
%token LOWER
%token WORD
%token NEWLINE
%token CHAR


/* Start of production section */
%%

list_option : | List;

List
  : item
  | List item
  ;

item
  : UPPER   { driver.add_upper(); }
  | LOWER   { driver.add_lower(); }
  | WORD    { driver.add_word( $1 ); }
  | NEWLINE { driver.add_newline(); }
  | CHAR    { driver.add_char(); }
  ;

%%


void splc::Parser::error(const location_type &l, const std::string &err_message)
{
   std::cerr << "Error: " << err_message << " at " << l << "\n";
}
