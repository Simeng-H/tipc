#pragma once

#include "ASTExpr.h"

/*! \brief Class for free expression
 */
class ASTFreeExpr : public ASTExpr {
  std::shared_ptr<ASTExpr> TARGET;
public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTFreeExpr(std::unique_ptr<ASTExpr> TARGET) : TARGET(std::move(TARGET)) {}
  ASTExpr* getTarget() const { return TARGET.get(); }
  void accept(ASTVisitor * visitor) override;
  llvm::Value* codegen() override;

protected:
  std::ostream& print(std::ostream &out) const override;
};
