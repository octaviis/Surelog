/*
 Copyright 2019 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   CompileExpression.cpp
 * Author: alain
 *
 * Created on May 14, 2019, 8:03 PM
 */
#include <iostream>
#include "Expression/Value.h"
#include "Expression/ExprBuilder.h"
#include "Design/Enum.h"
#include "Design/Function.h"
#include "Testbench/Property.h"
#include "SourceCompile/CompilationUnit.h"
#include "SourceCompile/PreprocessFile.h"
#include "SourceCompile/CompileSourceFile.h"
#include "SourceCompile/ParseFile.h"
#include "SourceCompile/Compiler.h"
#include "Design/Design.h"
#include "DesignCompile/CompileHelper.h"
#include "CompileDesign.h"
#include "uhdm.h"
#include "expr.h"
#include "UhdmWriter.h"

using namespace SURELOG;

UHDM::any* CompileHelper::compileExpression(FileContent* fC, NodeId parent, 
					     CompileDesign* compileDesign,
						 UHDM::expr* pexpr,
					     ValuedComponentI* instance) {
  UHDM::Serializer& s = compileDesign->getSerializer();
  UHDM::any* result = nullptr;
  NodeId child = fC->Child(parent);
  if (child) {
    VObjectType childType = fC->Type(child);
    switch (childType) {
	case VObjectType::slIncDec_PlusPlus: {
	  // Pre increment
      UHDM::operation* op = s.MakeOperation();
	  op->VpiOpType(vpiPreIncOp);
	  op->VpiParent(pexpr);
	  UHDM::VectorOfany* operands = s.MakeAnyVec();
	  if (UHDM::any* operand = compileExpression(fC, fC->Sibling(child), compileDesign, op, instance))
	    operands->push_back(operand);
	  op->Operands(operands);
	  result = op;
	  break;
	}
	case VObjectType::slIncDec_MinusMinus: {
	  // Pre decrement
      UHDM::operation* op = s.MakeOperation();
	  op->VpiOpType(vpiPreDecOp);
	  op->VpiParent(pexpr);
	  UHDM::VectorOfany* operands = s.MakeAnyVec();
	  if (UHDM::any* operand = compileExpression(fC, fC->Sibling(child), compileDesign, op, instance))
	    operands->push_back(operand);
	  op->Operands(operands);
	  result = op;
	  break;
	}
	case VObjectType::slUnary_Minus: {
      UHDM::operation* op = s.MakeOperation();
	  op->VpiOpType(vpiMinusOp);
	  op->VpiParent(pexpr);
	  UHDM::VectorOfany* operands = s.MakeAnyVec();
	  if (UHDM::any* operand = compileExpression(fC, fC->Sibling(child), compileDesign, op, instance))
	    operands->push_back(operand);
	  op->Operands(operands);
	  result = op;
	  break;
	}
	case VObjectType::slUnary_Plus: {
      UHDM::operation* op = s.MakeOperation();
	  op->VpiOpType(vpiPlusOp);
	  op->VpiParent(pexpr);
	  UHDM::VectorOfany* operands = s.MakeAnyVec();
	  if (UHDM::any* operand = compileExpression(fC, fC->Sibling(child), compileDesign, op, instance))
	    operands->push_back(operand);
	  op->Operands(operands);
	  result = op;
	  break;
	}
	case VObjectType::slUnary_Tilda: {
      UHDM::operation* op = s.MakeOperation();
	  op->VpiOpType(vpiBitNegOp);
	  op->VpiParent(pexpr);
	  UHDM::VectorOfany* operands = s.MakeAnyVec();
	  if (UHDM::any* operand = compileExpression(fC, fC->Sibling(child), compileDesign, op, instance))
	    operands->push_back(operand);
	  op->Operands(operands);
	  result = op;
	  break;
	}
	case VObjectType::slUnary_Not: {
      UHDM::operation* op = s.MakeOperation();
	  op->VpiOpType(vpiNotOp);
	  op->VpiParent(pexpr);
	  UHDM::VectorOfany* operands = s.MakeAnyVec();
	  if (UHDM::any* operand = compileExpression(fC, fC->Sibling(child), compileDesign, op, instance))
	    operands->push_back(operand);
	  op->Operands(operands);
	  result = op;
	  break;
	}
	case VObjectType::slEdge_Posedge: {
      UHDM::operation* op = s.MakeOperation();
	  op->VpiOpType(vpiPosedgeOp);
	  op->VpiParent(pexpr);
	  UHDM::VectorOfany* operands = s.MakeAnyVec();
	  if (UHDM::any* operand = compileExpression(fC, fC->Sibling(child), compileDesign, op, instance))
	    operands->push_back(operand);
	  op->Operands(operands);
	  result = op;
	  break;
	}
	case VObjectType::slEdge_Negedge: {
      UHDM::operation* op = s.MakeOperation();
	  op->VpiOpType(vpiNegedgeOp);
	  op->VpiParent(pexpr);
	  UHDM::VectorOfany* operands = s.MakeAnyVec();
	  if (UHDM::any* operand = compileExpression(fC, fC->Sibling(child), compileDesign, op, instance))
	    operands->push_back(operand);
	  op->Operands(operands);
	  result = op;
	  break;
	}
	case VObjectType::slConstant_primary:
	case VObjectType::slPrimary_literal:
	case VObjectType::slPrimary:
    case VObjectType::slConstant_mintypmax_expression:
    case VObjectType::slMintypmax_expression:
	case VObjectType::slSystem_task:
    case VObjectType::slParam_expression:  
	case VObjectType::slInc_or_dec_expression:
	case VObjectType::slHierarchical_identifier:
	case VObjectType::slExpression_or_cond_pattern:
	case VObjectType::slComplex_func_call:
      result = compileExpression(fC, child, compileDesign, pexpr, instance);
      break;
	case VObjectType::slEvent_expression: {
      NodeId subExpr = child;
	  UHDM::any* opL = compileExpression(fC, subExpr, compileDesign, pexpr, instance);	 
	  result = opL;
	  NodeId op = fC->Sibling(subExpr);
	  UHDM::operation* operation = nullptr; 
	  UHDM::VectorOfany* operands = nullptr;
	  while (op) {
		if (operation == nullptr) {
		  operation = s.MakeOperation();
		  operands = s.MakeAnyVec();
		  operation->Operands(operands);
		  operands->push_back(opL);
		  result = operation;
		}
        if (fC->Type(op) == VObjectType::slOr_operator) {
		  operation->VpiOpType(vpiEventOrOp);
          NodeId subRExpr = fC->Sibling(op);
	      UHDM::any* opR = compileExpression(fC, subRExpr, compileDesign, pexpr, instance);
		  operands->push_back(opR);	 
		  op = fC->Sibling(subRExpr);
		} else if (fC->Type(op) == VObjectType::slComma_operator) {
		  operation->VpiOpType(vpiListOp);
          NodeId subRExpr = fC->Sibling(op);
	      UHDM::any* opR = compileExpression(fC, subRExpr, compileDesign, pexpr, instance);
		  operands->push_back(opR);	 
		  op = fC->Sibling(subRExpr);
		}
	  }
	  break;  
	}
	case VObjectType::slExpression:  
	case VObjectType::slConstant_expression: {
	  UHDM::any* opL = compileExpression(fC, child, compileDesign, pexpr, instance);	  
      NodeId op = fC->Sibling(child);
      if (!op) {
        result = opL;
        break;
      }      
      UHDM::operation* operation = s.MakeOperation();
	  UHDM::VectorOfany* operands = s.MakeAnyVec();
	  result = operation;
	  operation->VpiParent(pexpr);
	  if (opL) {
	    opL->VpiParent(operation);
	    operands->push_back(opL);
	  }
	  operation->Operands(operands);
	  NodeId rval = fC->Sibling(op);
	  UHDM::any* opR = compileExpression(fC, rval, compileDesign, operation, instance);
	  if (opR) 	  
        operands->push_back(opR);  
      VObjectType opType = fC->Type(op);
	  unsigned int vopType = UhdmWriter::getVpiOpType(opType);
	  operation->VpiOpType(vopType); 
	  break;
	}
	case VObjectType::slSystem_task_names: {
	  NodeId n = fC->Child(child);
	  std::string name = fC->SymName(n).c_str();
	  UHDM::sys_func_call* sys = s.MakeSys_func_call();
	  sys->VpiName(name);
	  sys->VpiParent(pexpr);
	  result = sys;
	  break;
	}  
	case VObjectType::slVariable_lvalue: {
      UHDM::any* variable = compileExpression(fC, child, compileDesign, pexpr, instance);
      NodeId op = fC->Sibling(child);
	  if (op) {
		// Post increment/decrement  
        UHDM::operation* operation = s.MakeOperation();
	    UHDM::VectorOfany* operands = s.MakeAnyVec();
	    result = operation;
	    operation->VpiParent(pexpr);
		VObjectType opType = fC->Type(op);
	    unsigned int vopType = UhdmWriter::getVpiOpType(opType);
	    operation->VpiOpType(vopType);
		operation->Operands(operands);
		operands->push_back(variable); 	
	  } else {
		result = variable;  
	  }
	  break;
	}
    case VObjectType::slStringConst: {
      std::string name = fC->SymName(child).c_str();
	  NodeId rhs = child;
	  while ((rhs = fC->Sibling(rhs))) {
        if (fC->Type(rhs) == VObjectType::slStringConst) {
          name += "." + fC->SymName(rhs);   
	    } else if (fC->Type(rhs) == VObjectType::slSelect) {
		  NodeId Bit_select = fC->Child(rhs);
	 	  while (Bit_select) {
		    if (fC->Type(Bit_select) == VObjectType::slBit_select) {
			  if (NodeId bitexp = fC->Child(Bit_select)) {
			    UHDM::bit_select* bit_select = s.MakeBit_select();
			    bit_select->VpiName(name);
			    bit_select->VpiIndex((expr*) compileExpression(fC, bitexp, compileDesign, pexpr));
                result = bit_select;
			    return result;
		  	  }
		    } else if (fC->Type(Bit_select) == VObjectType::slPart_select_range) {
			  NodeId Constant_range = fC->Child(Bit_select);
			  if (fC->Type(Constant_range) == VObjectType::slConstant_range) {
		        NodeId Constant_expression = fC->Child(Constant_range);
			    UHDM::expr* lexp = (expr*) compileExpression(fC, Constant_expression, compileDesign, pexpr);
			    UHDM::expr* rexp = (expr*) compileExpression(fC, fC->Sibling(Constant_expression), compileDesign, pexpr);
			    UHDM::part_select* part_select = s.MakePart_select();
                part_select->Left_range(lexp);
			    part_select->Right_range(rexp);
                UHDM::ref_obj* ref = s.MakeRef_obj();  
		        ref->VpiName(name);
                part_select->VpiParent(ref);
			    part_select->VpiConstantSelect(true);
			    result = part_select;
			  } else {
				// constant_indexed_range
				NodeId Constant_expression = fC->Child(Constant_range);
			    UHDM::expr* lexp = (expr*) compileExpression(fC, Constant_expression, compileDesign, pexpr);
				NodeId op = fC->Sibling(Constant_expression);
			    UHDM::expr* rexp = (expr*) compileExpression(fC, fC->Sibling(op), compileDesign, pexpr);
			    UHDM::indexed_part_select* part_select = s.MakeIndexed_part_select();
                part_select->Base_expr(lexp);
			    part_select->Width_expr(rexp);
				if (fC->Type(op) == VObjectType::slIncPartSelectOp)
				  part_select->VpiIndexedPartSelectType(vpiPosIndexed);
				else
				  part_select->VpiIndexedPartSelectType(vpiNegIndexed);
                UHDM::ref_obj* ref = s.MakeRef_obj();  
		        ref->VpiName(name);
                part_select->VpiParent(ref);
			    part_select->VpiConstantSelect(true);
			    result = part_select;
			  }
			  return result;
			}
		    Bit_select = fC->Sibling(Bit_select);
		  }
		}
	  }

      Value* sval = NULL;
      if (instance) 
        sval = instance->getValue(name);        
      if (sval == NULL) {   
		UHDM::ref_obj* ref = s.MakeRef_obj();  
		ref->VpiName(name);
		ref->VpiParent(pexpr);
		result = ref;   
      } else {
        UHDM::constant* c = s.MakeConstant();
		c->VpiValue(sval->uhdmValue());
		result = c;
	  }
	  break;
	}
	case VObjectType::slIntConst: {
	  // Do not evaluate the constant, keep it as in the source text:
	  //Value* val = m_exprBuilder.evalExpr(fC,parent,instance,true);
	  //if (val->isValid()) {
	  //UHDM::constant* c = s.MakeConstant();
	  //c->VpiValue(val->uhdmValue());
	  //result = c;
	  //}
	  //m_exprBuilder.deleteValue(val);
	  UHDM::constant* c = s.MakeConstant();
	  std::string value = fC->SymName(child);
	  if (strstr(value.c_str(), "'h"))
	    value = "HEX:" + value;
	  else if (strstr(value.c_str(), "'b"))
	    value = "BIN:" + value;
	  else if (strstr(value.c_str(), "'o"))
	    value = "OCT:" + value;	
	  else if (strstr(value.c_str(), "'"))
	    value = "BIN:" + value;
	  else 
	    value = "INT:" + value;			
	  c->VpiValue(value);
	  result = c;
      break;
    }
	case VObjectType::slRealConst: {
	  UHDM::constant* c = s.MakeConstant();
	  std::string value = fC->SymName(child);
	  value = "REAL:" + value;	
	  c->VpiValue(value);
	  result = c;
	  break;		
	}
	case VObjectType::slNumber_1Tickb1:
    case VObjectType::slNumber_1TickB1:
    case VObjectType::slNumber_Tickb1:
    case VObjectType::slNumber_TickB1:
    case VObjectType::slNumber_Tick1: 
	case VObjectType::slNumber_1Tickb0:
    case VObjectType::slNumber_1TickB0:
    case VObjectType::slNumber_Tickb0:
    case VObjectType::slNumber_TickB0:
    case VObjectType::slNumber_Tick0: {
	  UHDM::constant* c = s.MakeConstant();
	  std::string value = fC->SymName(child);
	  value = "SCAL:" + value;	
	  c->VpiValue(value);
	  result = c;
	  break;	
	}
	case VObjectType::slStringLiteral: {
	  UHDM::constant* c = s.MakeConstant();
	  std::string value = fC->SymName(child);
	  value = "STRING:" + value;	
	  c->VpiValue(value);
	  result = c;
	  break;
	}
	case VObjectType::slConcatenation: {
	  UHDM::operation* operation = s.MakeOperation();
	  UHDM::VectorOfany* operands = s.MakeAnyVec();
	  result = operation;
	  operation->VpiParent(pexpr);
	  operation->Operands(operands);
	  operation->VpiOpType(vpiConcatOp);
	  NodeId Expression = fC->Child(child);
	  while (Expression) {
		  UHDM::any* exp = compileExpression(fC, Expression, compileDesign);
          if (exp)
		    operands->push_back(exp);
		  Expression = fC->Sibling(Expression);
	  }	
	  break;
	}
	case VObjectType::slMultiple_concatenation: {
	  UHDM::operation* operation = s.MakeOperation();
	  UHDM::VectorOfany* operands = s.MakeAnyVec();
	  result = operation;
	  operation->VpiParent(pexpr);
	  operation->Operands(operands);
	  operation->VpiOpType(vpiMultiConcatOp);
	  NodeId Expression = fC->Child(child);
	  while (Expression) {
		  UHDM::any* exp = compileExpression(fC, Expression, compileDesign);
          if (exp)
		    operands->push_back(exp);
		  Expression = fC->Sibling(Expression);
	  }	
	  break;	
	}
	default:
	  break;
	}
  } else {
	VObjectType type = fC->Type(parent);
    switch (type) {
    case VObjectType::slStringConst: {
      std::string name = fC->SymName(parent).c_str();
      Value* sval = NULL;
      if (instance) 
	    sval = instance->getValue(name);
      if (sval == NULL) {   
		NodeId op = fC->Sibling(parent);
		if (op) {
		  UHDM::operation* operation = s.MakeOperation();
	      UHDM::VectorOfany* operands = s.MakeAnyVec();
	      result = operation;
	      operation->VpiParent(pexpr);
		  VObjectType opType = fC->Type(op);
	      unsigned int vopType = UhdmWriter::getVpiOpType(opType);
	      operation->VpiOpType(vopType);
		  operation->Operands(operands);
		  UHDM::ref_obj* ref = s.MakeRef_obj();  
	  	  ref->VpiName(name);
		  ref->VpiParent(operation);
		  operands->push_back(ref); 	
		} else {
		  UHDM::ref_obj* ref = s.MakeRef_obj();  
		  ref->VpiName(name);
		  ref->VpiParent(pexpr);
		  result = ref;   	
		}
      } else {
        UHDM::constant* c = s.MakeConstant();
		c->VpiValue(sval->uhdmValue());
		result = c;
	  }
      break;
	}
    default:
      break;  
	}
  }

  return result;
}
