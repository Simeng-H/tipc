#include "catch.hpp"
#include "TipInt.h"
#include "TipVar.h"

TEST_CASE("TipVar: test TipVars objects with the same underlying node are equal" "[TipVar]") {
    AST::NumberExpr n(42);
    TipVar var(&n);
    TipVar var2(&n);
    REQUIRE(var == var2);
}

TEST_CASE("TipVar: test TipVars objects with different underlying node are not equal" "[TipVar]") {
    AST::NumberExpr n(99);
    AST::NumberExpr n1(99);
    TipVar var(&n);
    TipVar var2(&n1);
    REQUIRE_FALSE(var == var2);
}

TEST_CASE("TipVar: test TipVar is a Var" "[TipVar]") {
    AST::NumberExpr n(42);
    TipVar var(&n);
    REQUIRE_FALSE(nullptr == dynamic_cast<TipVar *>(&var));
}

TEST_CASE("TipVar: test TipVar is a TipType" "[TipVar]") {
    AST::NumberExpr n(42);
    TipVar var(&n);
    REQUIRE_FALSE(nullptr == dynamic_cast<TipType *>(&var));
}
