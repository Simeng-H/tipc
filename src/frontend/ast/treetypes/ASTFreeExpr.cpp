#include "ASTFreeExpr.h"
#include "ASTVisitor.h"

void ASTFreeExpr::accept(ASTVisitor * visitor) {
  if (visitor->visit(this)) {
    getTarget()->accept(visitor);
  }
  visitor->endVisit(this);
}

std::ostream& ASTFreeExpr::print(std::ostream &out) const {
  out << "free " << *getTarget();
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTFreeExpr::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  children.push_back(TARGET);
  return children;
}
