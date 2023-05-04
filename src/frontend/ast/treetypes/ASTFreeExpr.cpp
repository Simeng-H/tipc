#include "ASTFreeExpr.h"
#include "ASTVisitor.h"

void ASTFreeStmt::accept(ASTVisitor * visitor) {
  if (visitor->visit(this)) {
    getArg()->accept(visitor);
  }
  visitor->endVisit(this);
}

std::ostream& ASTFreeStmt::print(std::ostream &out) const {
  out << "free " << *getArg() << ";";
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTFreeStmt::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  children.push_back(ARG);
  return children;
}
