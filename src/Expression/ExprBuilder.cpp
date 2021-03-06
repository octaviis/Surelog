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
 * File:   ExprBuilder.cpp
 * Author: alain
 *
 * Created on November 2, 2017, 9:45 PM
 */
#include <stdint.h>

#include <iostream>
#include <sstream>
#include <string.h>
#include <math.h>
#include "ErrorReporting/ErrorContainer.h"
#include "Expression/ExprBuilder.h"
#include "SourceCompile/VObjectTypes.h"

using namespace SURELOG;

ExprBuilder::ExprBuilder() {
  m_symbols = NULL;
  m_errors = NULL;
}

ExprBuilder::ExprBuilder(const ExprBuilder& orig) {}

ExprBuilder::~ExprBuilder() {}

Value* ExprBuilder::clone(Value* val) {
  Value* clone = NULL;
  bool isLValue = val->isLValue();
  if (val->getType() == Value::Type::String) {
    clone = m_valueFactory.newValue(*(StValue*)val);
  } else if (isLValue) {
    clone = m_valueFactory.newValue(*(LValue*)val);
  } else {
    clone = m_valueFactory.newValue(*(SValue*)val);
  }
  return clone;
}

Value* ExprBuilder::evalExpr(FileContent* fC, NodeId parent,
                             ValuedComponentI* instance, bool muteErrors) {
  Value* value = m_valueFactory.newLValue();
  NodeId child = fC->Child(parent);
  if (child) {
    VObjectType childType = fC->Type(child);
    switch (childType) {
      case VObjectType::slIncDec_PlusPlus:  {
        // Pre increment
        NodeId sibling = fC->Sibling(child);
        Value* tmp = evalExpr(fC, sibling, instance, muteErrors);
        value->u_plus(tmp);
        value->incr();
        m_valueFactory.deleteValue(tmp);
        break;
      }
      case VObjectType::slIncDec_MinusMinus:  {
        // Pre decrement
        NodeId sibling = fC->Sibling(child);
        Value* tmp = evalExpr(fC, sibling, instance, muteErrors);
        value->u_plus(tmp);
        value->decr();
        m_valueFactory.deleteValue(tmp);
        break;
      }
      case VObjectType::slUnary_Minus: {
        NodeId sibling = fC->Sibling(child);
        Value* tmp = evalExpr(fC, sibling, instance, muteErrors);
        value->u_minus(tmp);
        m_valueFactory.deleteValue(tmp);
        break;
      }
      case VObjectType::slUnary_Plus: {
        NodeId sibling = fC->Sibling(child);
        Value* tmp = evalExpr(fC, sibling, instance, muteErrors);
        value->u_plus(tmp);
        m_valueFactory.deleteValue(tmp);
        break;
      }
      case VObjectType::slUnary_Not: {
        NodeId sibling = fC->Sibling(child);
        Value* tmp = evalExpr(fC, sibling, instance, muteErrors);
        value->u_not(tmp);
        m_valueFactory.deleteValue(tmp);
        break;
      }
      case VObjectType::slUnary_Tilda: {
        NodeId sibling = fC->Sibling(child);
        Value* tmp = evalExpr(fC, sibling, instance, muteErrors);
        value->u_tilda(tmp);
        m_valueFactory.deleteValue(tmp);
        break;
      }
      case VObjectType::slConstant_primary:
        m_valueFactory.deleteValue(value);
        value = evalExpr(fC, child, instance, muteErrors);
        break;
      case VObjectType::slPrimary_literal:
        m_valueFactory.deleteValue(value);
        value = evalExpr(fC, child, instance, muteErrors);
        break;
      case VObjectType::slPrimary:
        m_valueFactory.deleteValue(value);
        value = evalExpr(fC, child, instance, muteErrors);
        break;
      case VObjectType::slExpression:
        m_valueFactory.deleteValue(value);
        value = evalExpr(fC, child, instance, muteErrors);
        break;
      case VObjectType::slInc_or_dec_expression:
        m_valueFactory.deleteValue(value);
        value = evalExpr(fC, child, instance, muteErrors);
        break;
      case VObjectType::slConstant_mintypmax_expression:
        m_valueFactory.deleteValue(value);
        value = evalExpr(fC, child, instance, muteErrors);
        break;
      case VObjectType::slMintypmax_expression:
        m_valueFactory.deleteValue(value);
        value = evalExpr(fC, child, instance, muteErrors);
        break;
      case VObjectType::slParam_expression:
        m_valueFactory.deleteValue(value);
        value = evalExpr(fC, child, instance, muteErrors);
        break;
      case VObjectType::slHierarchical_identifier:  {
        m_valueFactory.deleteValue(value);
        value = evalExpr(fC, child, instance, muteErrors);
        break; 
      }
      case VObjectType::slConstant_expression: {
        Value* valueL = evalExpr(fC, child, instance, muteErrors);
        NodeId op = fC->Sibling(child);
        if (!op) {
          m_valueFactory.deleteValue(value);
          value = valueL;
          break;
        }
        VObjectType opType = fC->Type(op);
        switch (opType) {
          case VObjectType::slBinOp_Plus: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->plus(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_Minus: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->minus(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_Mult: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->mult(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_Div: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->div(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_Great: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->greater(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_GreatEqual: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->greater_equal(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_Less: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->lesser(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_LessEqual: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->lesser_equal(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_Equiv: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->equiv(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_Not: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->notEqual(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_Percent: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->mod(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_LogicAnd: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->logAnd(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_LogicOr: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->logOr(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_BitwAnd: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->bitwAnd(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_BitwOr: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->bitwOr(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_BitwXor: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->bitwXor(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_ShiftLeft: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->shiftLeft(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          case VObjectType::slBinOp_ShiftRight: {
            NodeId rval = fC->Sibling(op);
            Value* valueR = evalExpr(fC, rval, instance, muteErrors);
            value->shiftRight(valueL, valueR);
            m_valueFactory.deleteValue(valueL);
            m_valueFactory.deleteValue(valueR);
            break;
          }
          default:
            m_valueFactory.deleteValue(value);
            value = valueL;
            break;
        }
      } break;
      case VObjectType::slIntConst: {
        std::string val = fC->SymName(child);
        if (strstr(val.c_str(), "'")) {
          uint64_t hex_value = 0;
          char base = 'h';
          unsigned int i = 0;
          for (i = 0; i < val.size(); i++) {
            if (val[i] == '\'') {
              base = val[i + 1];
              break;
            }
          }
          std::string v = val.substr(i + 2);
          switch (base) {
            case 'h':
              hex_value = std::strtoul(v.c_str(), 0, 16);
              break;
            case 'b':
              hex_value = std::strtoul(v.c_str(), 0, 2);
              break;
            case 'o':
              hex_value = std::strtoul(v.c_str(), 0, 8);
              break;
            case 'd':
              hex_value = std::strtoul(v.c_str(), 0, 10);
              break;
            default:
              break;
          }
          value->set(hex_value);
        } else {
          value->set((int64_t)atol(val.c_str()));
        }
        break;
      }
      case VObjectType::slRealConst: {
        std::string real = fC->SymName(child).c_str();
        std::istringstream os(real);
        double d;
        os >> d;
        value->set(d);
        break;
      }
      case VObjectType::slNull_keyword: {
        value->set((uint64_t)0);
        break;
      }
      case VObjectType::slStringConst: {
        std::string name = fC->SymName(child).c_str();
        Value* sval = NULL;
        if (instance) 
          sval = instance->getValue(name);
        
        if (sval == NULL) {
          if (muteErrors == false) {
            Location loc(fC->getFileId(child), fC->Line(child), 0,
                         m_symbols->registerSymbol(name));
            Error err(ErrorDefinition::ELAB_UNDEF_VARIABLE, loc);
            m_errors->addError(err);
          }
          value->setInvalid();
          break;
        }
        if (sval->getType() == Value::Type::String) {
          m_valueFactory.deleteValue(value);
          value = sval;
        } else {
          value->u_plus(sval);
        }
        break;
      }
      case VObjectType::slStringLiteral: {
        std::string name = fC->SymName(child).c_str();
        m_valueFactory.deleteValue(value);
        value = m_valueFactory.newStValue();
        value->set(name);
        break;
      }
      case VObjectType::slNumber_1Tickb0:
      case VObjectType::slNumber_1TickB0:
      case VObjectType::slNumber_Tickb0:
      case VObjectType::slNumber_TickB0:
      case VObjectType::slNumber_Tick0:  {
        value->set(0,Value::Type::Scalar, 1);
        break;
      }
      case VObjectType::slNumber_1Tickb1:
      case VObjectType::slNumber_1TickB1:
      case VObjectType::slNumber_Tickb1:
      case VObjectType::slNumber_TickB1:
      case VObjectType::slNumber_Tick1:  {
        value->set(1,Value::Type::Scalar, 1);
        break;
      }
      case VObjectType::slVariable_lvalue: {
        Value* variableVal = evalExpr(fC, child, instance, muteErrors);
        NodeId sibling = fC->Sibling(child);
        if (sibling) {
          VObjectType opType = fC->Type(sibling);
          if (opType == VObjectType::slIncDec_PlusPlus)
            variableVal->incr();
          else if (opType == VObjectType::slIncDec_MinusMinus)
            variableVal->decr();
        } 
        m_valueFactory.deleteValue(value);
        value = variableVal;
        break;
      }
      default:
        value->setInvalid();
        break;
    }
  } else {
    VObjectType type = fC->Type(parent);
    switch (type) {
      case VObjectType::slStringConst: {
        std::string name = fC->SymName(parent).c_str();
        Value* sval = NULL;
        if (instance) sval = instance->getValue(name);
        if (sval == NULL) {
          if (muteErrors == false) {
            Location loc(fC->getFileId(child), fC->Line(child), 0,
                         m_symbols->registerSymbol(name));
            Error err(ErrorDefinition::ELAB_UNDEF_VARIABLE, loc);
            m_errors->addError(err);
          }
          value->setInvalid();
          break;
        }
        NodeId op = fC->Sibling(parent);
        VObjectType op_type = fC->Type(op);
        switch (op_type) {
          case VObjectType::slIncDec_PlusPlus:
            value->u_plus(sval);
            value->incr();
            return value;
          case VObjectType::slIncDec_MinusMinus:
            value->u_plus(sval);
            value->decr();
            return value;
          default:
            break;
        }
        break;
      }
      default:
        break;
    }

    value->setInvalid();
  }
  return value;
}
