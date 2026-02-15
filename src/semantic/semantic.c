/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Semantic Analyzer Implementation
 * 
 * Performs type checking, scope analysis, and validates the program
 * follows NatureLang's semantic rules.
 */

#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * ANALYZER CONTEXT
 * ============================================================================
 */
typedef struct {
    SymbolTable *symtab;
    bool had_error;
} AnalyzerContext;

/* Forward declarations for recursive analysis */
static void analyze_node(AnalyzerContext *ctx, ASTNode *node);
static void analyze_statement(AnalyzerContext *ctx, ASTNode *node);
static DataType analyze_expression(AnalyzerContext *ctx, ASTNode *node);

/* ============================================================================
 * TYPE UTILITIES
 * ============================================================================
 */

const char *datatype_to_string(DataType type) {
    switch (type) {
        case TYPE_NUMBER:   return "number";
        case TYPE_DECIMAL:  return "decimal";
        case TYPE_TEXT:     return "text";
        case TYPE_FLAG:     return "flag";
        case TYPE_LIST:     return "list";
        case TYPE_NOTHING:  return "nothing";
        case TYPE_FUNCTION: return "function";
        case TYPE_UNKNOWN:  return "unknown";
        default:            return "?";
    }
}

const char *operator_to_string(Operator op) {
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_MOD: return "%";
        case OP_POW: return "^";
        case OP_EQ:  return "==";
        case OP_NEQ: return "!=";
        case OP_LT:  return "<";
        case OP_GT:  return ">";
        case OP_LTE: return "<=";
        case OP_GTE: return ">=";
        case OP_AND: return "and";
        case OP_OR:  return "or";
        case OP_NOT: return "not";
        case OP_NEG: return "-";
        case OP_POS: return "+";
        case OP_BETWEEN: return "between";
        default:     return "?";
    }
}

bool types_compatible(DataType target, DataType source) {
    if (target == source) return true;
    
    /* Number and decimal are compatible */
    if ((target == TYPE_NUMBER || target == TYPE_DECIMAL) &&
        (source == TYPE_NUMBER || source == TYPE_DECIMAL)) {
        return true;
    }
    
    /* Unknown type is compatible with anything (error recovery) */
    if (target == TYPE_UNKNOWN || source == TYPE_UNKNOWN) {
        return true;
    }
    
    return false;
}

bool type_is_numeric(DataType type) {
    return type == TYPE_NUMBER || type == TYPE_DECIMAL || type == TYPE_UNKNOWN;
}

bool type_is_boolean(DataType type) {
    return type == TYPE_FLAG || type == TYPE_UNKNOWN;
}

DataType get_binary_op_result_type(Operator op, DataType left, DataType right) {
    switch (op) {
        /* Arithmetic operators return numeric type */
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_POW:
            /* If either is decimal, result is decimal */
            if (left == TYPE_DECIMAL || right == TYPE_DECIMAL) {
                return TYPE_DECIMAL;
            }
            return TYPE_NUMBER;
        
        case OP_MOD:
            return TYPE_NUMBER;  /* Modulo always returns integer */
        
        /* Comparison operators return boolean */
        case OP_EQ:
        case OP_NEQ:
        case OP_LT:
        case OP_GT:
        case OP_LTE:
        case OP_GTE:
        case OP_BETWEEN:
            return TYPE_FLAG;
        
        /* Logical operators return boolean */
        case OP_AND:
        case OP_OR:
            return TYPE_FLAG;
        
        default:
            return TYPE_UNKNOWN;
    }
}

DataType get_unary_op_result_type(Operator op, DataType operand) {
    switch (op) {
        case OP_NEG:
        case OP_POS:
            return operand;  /* Preserve numeric type */
        
        case OP_NOT:
            return TYPE_FLAG;
        
        default:
            return TYPE_UNKNOWN;
    }
}

/* ============================================================================
 * EXPRESSION ANALYSIS
 * ============================================================================
 */

static DataType analyze_expression(AnalyzerContext *ctx, ASTNode *node) {
    if (!node) return TYPE_UNKNOWN;
    
    switch (node->type) {
        case AST_LITERAL_INT:
            node->data_type = TYPE_NUMBER;
            return TYPE_NUMBER;
        
        case AST_LITERAL_FLOAT:
            node->data_type = TYPE_DECIMAL;
            return TYPE_DECIMAL;
        
        case AST_LITERAL_STRING:
            node->data_type = TYPE_TEXT;
            return TYPE_TEXT;
        
        case AST_LITERAL_BOOL:
            node->data_type = TYPE_FLAG;
            return TYPE_FLAG;
        
        case AST_IDENTIFIER: {
            Symbol *sym = symtab_lookup(ctx->symtab, node->data.identifier.name);
            if (!sym) {
                symtab_error(ctx->symtab, node->loc,
                            "Undefined variable '%s'", node->data.identifier.name);
                ctx->had_error = true;
                node->data_type = TYPE_UNKNOWN;
                return TYPE_UNKNOWN;
            }
            
            /* Warn if using uninitialized variable */
            if (!sym->is_initialized && sym->kind != SYMBOL_PARAMETER) {
                symtab_warning(ctx->symtab, node->loc,
                              "Variable '%s' may be used before initialization",
                              node->data.identifier.name);
            }
            
            node->data_type = sym->type;
            return sym->type;
        }
        
        case AST_BINARY_OP: {
            DataType left_type = analyze_expression(ctx, node->data.binary_op.left);
            DataType right_type = analyze_expression(ctx, node->data.binary_op.right);
            Operator op = node->data.binary_op.op;
            
            /* Check type compatibility based on operator */
            switch (op) {
                case OP_ADD:
                    /* Allow string concatenation */
                    if (left_type == TYPE_TEXT || right_type == TYPE_TEXT) {
                        node->data_type = TYPE_TEXT;
                        return TYPE_TEXT;
                    }
                    /* FALLTHROUGH - for numeric addition */
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                case OP_MOD:
                case OP_POW:
                    if (!type_is_numeric(left_type)) {
                        symtab_error(ctx->symtab, node->loc,
                                    "Left operand of '%s' must be numeric, got %s",
                                    operator_to_string(op), datatype_to_string(left_type));
                        ctx->had_error = true;
                    }
                    if (!type_is_numeric(right_type)) {
                        symtab_error(ctx->symtab, node->loc,
                                    "Right operand of '%s' must be numeric, got %s",
                                    operator_to_string(op), datatype_to_string(right_type));
                        ctx->had_error = true;
                    }
                    break;
                
                case OP_AND:
                case OP_OR:
                    if (!type_is_boolean(left_type)) {
                        symtab_error(ctx->symtab, node->loc,
                                    "Left operand of '%s' must be boolean, got %s",
                                    operator_to_string(op), datatype_to_string(left_type));
                        ctx->had_error = true;
                    }
                    if (!type_is_boolean(right_type)) {
                        symtab_error(ctx->symtab, node->loc,
                                    "Right operand of '%s' must be boolean, got %s",
                                    operator_to_string(op), datatype_to_string(right_type));
                        ctx->had_error = true;
                    }
                    break;
                
                case OP_EQ:
                case OP_NEQ:
                    /* Any types can be compared for equality */
                    break;
                
                case OP_LT:
                case OP_GT:
                case OP_LTE:
                case OP_GTE:
                    /* Comparison requires compatible types */
                    if (!types_compatible(left_type, right_type)) {
                        symtab_error(ctx->symtab, node->loc,
                                    "Cannot compare %s with %s",
                                    datatype_to_string(left_type),
                                    datatype_to_string(right_type));
                        ctx->had_error = true;
                    }
                    break;
                
                default:
                    break;
            }
            
            node->data_type = get_binary_op_result_type(op, left_type, right_type);
            return node->data_type;
        }
        
        case AST_UNARY_OP: {
            DataType operand_type = analyze_expression(ctx, node->data.unary_op.operand);
            Operator op = node->data.unary_op.op;
            
            if ((op == OP_NEG || op == OP_POS) && !type_is_numeric(operand_type)) {
                symtab_error(ctx->symtab, node->loc,
                            "Unary '%s' requires numeric operand, got %s",
                            operator_to_string(op), datatype_to_string(operand_type));
                ctx->had_error = true;
            }
            
            if (op == OP_NOT && !type_is_boolean(operand_type)) {
                symtab_error(ctx->symtab, node->loc,
                            "'not' requires boolean operand, got %s",
                            datatype_to_string(operand_type));
                ctx->had_error = true;
            }
            
            node->data_type = get_unary_op_result_type(op, operand_type);
            return node->data_type;
        }
        
        case AST_TERNARY_OP: {
            /* "is between" operator */
            DataType operand_type = analyze_expression(ctx, node->data.ternary_op.operand);
            DataType lower_type = analyze_expression(ctx, node->data.ternary_op.lower);
            DataType upper_type = analyze_expression(ctx, node->data.ternary_op.upper);
            
            if (!type_is_numeric(operand_type)) {
                symtab_error(ctx->symtab, node->loc,
                            "'is between' requires numeric operand, got %s",
                            datatype_to_string(operand_type));
                ctx->had_error = true;
            }
            if (!type_is_numeric(lower_type)) {
                symtab_error(ctx->symtab, node->loc,
                            "'is between' lower bound must be numeric, got %s",
                            datatype_to_string(lower_type));
                ctx->had_error = true;
            }
            if (!type_is_numeric(upper_type)) {
                symtab_error(ctx->symtab, node->loc,
                            "'is between' upper bound must be numeric, got %s",
                            datatype_to_string(upper_type));
                ctx->had_error = true;
            }
            
            node->data_type = TYPE_FLAG;
            return TYPE_FLAG;
        }
        
        case AST_FUNC_CALL: {
            Symbol *func = symtab_lookup_function(ctx->symtab, node->data.func_call.name);
            if (!func) {
                symtab_error(ctx->symtab, node->loc,
                            "Undefined function '%s'", node->data.func_call.name);
                ctx->had_error = true;
                node->data_type = TYPE_UNKNOWN;
                return TYPE_UNKNOWN;
            }
            
            /* Check argument count */
            size_t expected_args = func->func_info.params ? func->func_info.params->count : 0;
            size_t actual_args = node->data.func_call.args ? node->data.func_call.args->count : 0;
            
            if (expected_args != actual_args) {
                symtab_error(ctx->symtab, node->loc,
                            "Function '%s' expects %zu arguments, got %zu",
                            node->data.func_call.name, expected_args, actual_args);
                ctx->had_error = true;
            }
            
            /* Analyze argument types */
            if (node->data.func_call.args) {
                for (size_t i = 0; i < node->data.func_call.args->count; i++) {
                    DataType arg_type = analyze_expression(ctx, 
                                                          node->data.func_call.args->nodes[i]);
                    
                    /* Check type compatibility with parameter if we have param info */
                    if (func->func_info.params && i < func->func_info.params->count) {
                        ASTNode *param = func->func_info.params->nodes[i];
                        if (param && param->type == AST_PARAM_DECL) {
                            DataType param_type = param->data.param_decl.param_type;
                            if (!types_compatible(param_type, arg_type)) {
                                symtab_error(ctx->symtab, node->loc,
                                            "Argument %zu type mismatch: expected %s, got %s",
                                            i + 1, datatype_to_string(param_type),
                                            datatype_to_string(arg_type));
                                ctx->had_error = true;
                            }
                        }
                    }
                }
            }
            
            node->data_type = func->func_info.return_type;
            return func->func_info.return_type;
        }
        
        case AST_INDEX: {
            DataType array_type = analyze_expression(ctx, node->data.index_expr.array);
            DataType index_type = analyze_expression(ctx, node->data.index_expr.index);
            
            if (array_type != TYPE_LIST && array_type != TYPE_TEXT && 
                array_type != TYPE_UNKNOWN) {
                symtab_error(ctx->symtab, node->loc,
                            "Cannot index into %s (expected list or text)",
                            datatype_to_string(array_type));
                ctx->had_error = true;
            }
            
            if (!type_is_numeric(index_type)) {
                symtab_error(ctx->symtab, node->loc,
                            "Index must be numeric, got %s",
                            datatype_to_string(index_type));
                ctx->had_error = true;
            }
            
            /* Result type depends on what we're indexing */
            if (array_type == TYPE_TEXT) {
                node->data_type = TYPE_TEXT;  /* Single character as text */
            } else {
                node->data_type = TYPE_UNKNOWN;  /* List element type unknown */
            }
            return node->data_type;
        }
        
        case AST_LIST: {
            /* Analyze all list elements */
            if (node->data.list_literal.elements) {
                for (size_t i = 0; i < node->data.list_literal.elements->count; i++) {
                    analyze_expression(ctx, node->data.list_literal.elements->nodes[i]);
                }
            }
            node->data_type = TYPE_LIST;
            return TYPE_LIST;
        }
        
        default:
            return TYPE_UNKNOWN;
    }
}

/* ============================================================================
 * STATEMENT ANALYSIS
 * ============================================================================
 */

static void analyze_statement(AnalyzerContext *ctx, ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_VAR_DECL: {
            const char *error = symtab_declare_variable(
                ctx->symtab,
                node->data.var_decl.name,
                node->data.var_decl.var_type,
                node->data.var_decl.is_const,
                node->loc
            );
            
            if (error) {
                symtab_error(ctx->symtab, node->loc, "%s", error);
                ctx->had_error = true;
            }
            
            /* Analyze initializer if present */
            if (node->data.var_decl.initializer) {
                DataType init_type = analyze_expression(ctx, node->data.var_decl.initializer);
                
                /* Check type compatibility */
                if (!types_compatible(node->data.var_decl.var_type, init_type)) {
                    symtab_error(ctx->symtab, node->loc,
                                "Cannot initialize %s variable with %s value",
                                datatype_to_string(node->data.var_decl.var_type),
                                datatype_to_string(init_type));
                    ctx->had_error = true;
                }
                
                /* Mark as initialized */
                Symbol *sym = symtab_lookup(ctx->symtab, node->data.var_decl.name);
                if (sym) symtab_mark_initialized(sym);
            }
            break;
        }
        
        case AST_FUNC_DECL: {
            /* Declare function in current scope */
            const char *error = symtab_declare_function(
                ctx->symtab,
                node->data.func_decl.name,
                node->data.func_decl.params,
                node->data.func_decl.return_type,
                node->loc
            );
            
            if (error) {
                symtab_error(ctx->symtab, node->loc, "%s", error);
                ctx->had_error = true;
            }
            
            /* Enter function scope */
            symtab_enter_function_scope(ctx->symtab, node->data.func_decl.return_type);
            
            /* Declare parameters */
            if (node->data.func_decl.params) {
                for (size_t i = 0; i < node->data.func_decl.params->count; i++) {
                    ASTNode *param = node->data.func_decl.params->nodes[i];
                    if (param && param->type == AST_PARAM_DECL) {
                        error = symtab_declare_parameter(
                            ctx->symtab,
                            param->data.param_decl.name,
                            param->data.param_decl.param_type,
                            param->loc
                        );
                        if (error) {
                            symtab_error(ctx->symtab, param->loc, "%s", error);
                            ctx->had_error = true;
                        }
                    }
                }
            }
            
            /* Analyze function body */
            if (node->data.func_decl.body) {
                analyze_node(ctx, node->data.func_decl.body);
            }
            
            /* Exit function scope */
            symtab_exit_scope(ctx->symtab);
            break;
        }
        
        case AST_ASSIGN: {
            /* Analyze target (must be an identifier) */
            if (node->data.assign.target->type == AST_IDENTIFIER) {
                const char *name = node->data.assign.target->data.identifier.name;
                Symbol *sym = symtab_lookup(ctx->symtab, name);
                
                if (!sym) {
                    symtab_error(ctx->symtab, node->loc,
                                "Undefined variable '%s'", name);
                    ctx->had_error = true;
                } else if (sym->kind == SYMBOL_CONSTANT) {
                    symtab_error(ctx->symtab, node->loc,
                                "Cannot assign to constant '%s'", name);
                    ctx->had_error = true;
                } else if (sym->kind == SYMBOL_FUNCTION) {
                    symtab_error(ctx->symtab, node->loc,
                                "Cannot assign to function '%s'", name);
                    ctx->had_error = true;
                } else {
                    /* Check type compatibility */
                    DataType value_type = analyze_expression(ctx, node->data.assign.value);
                    if (!types_compatible(sym->type, value_type)) {
                        symtab_error(ctx->symtab, node->loc,
                                    "Cannot assign %s to %s variable '%s'",
                                    datatype_to_string(value_type),
                                    datatype_to_string(sym->type), name);
                        ctx->had_error = true;
                    }
                    symtab_mark_initialized(sym);
                }
            } else {
                /* Index assignment */
                analyze_expression(ctx, node->data.assign.target);
                analyze_expression(ctx, node->data.assign.value);
            }
            break;
        }
        
        case AST_IF: {
            DataType cond_type = analyze_expression(ctx, node->data.if_stmt.condition);
            
            /* Condition should be boolean (or numeric for truthiness) */
            if (cond_type != TYPE_FLAG && !type_is_numeric(cond_type) && 
                cond_type != TYPE_UNKNOWN) {
                symtab_warning(ctx->symtab, node->loc,
                              "Condition is %s, expected flag (boolean)",
                              datatype_to_string(cond_type));
            }
            
            /* Analyze then branch */
            symtab_enter_scope(ctx->symtab);
            analyze_node(ctx, node->data.if_stmt.then_branch);
            symtab_exit_scope(ctx->symtab);
            
            /* Analyze else branch if present */
            if (node->data.if_stmt.else_branch) {
                symtab_enter_scope(ctx->symtab);
                analyze_node(ctx, node->data.if_stmt.else_branch);
                symtab_exit_scope(ctx->symtab);
            }
            break;
        }
        
        case AST_WHILE: {
            DataType cond_type = analyze_expression(ctx, node->data.while_stmt.condition);
            
            if (cond_type != TYPE_FLAG && !type_is_numeric(cond_type) &&
                cond_type != TYPE_UNKNOWN) {
                symtab_warning(ctx->symtab, node->loc,
                              "While condition is %s, expected flag (boolean)",
                              datatype_to_string(cond_type));
            }
            
            symtab_enter_loop_scope(ctx->symtab);
            analyze_node(ctx, node->data.while_stmt.body);
            symtab_exit_scope(ctx->symtab);
            break;
        }
        
        case AST_REPEAT: {
            DataType count_type = analyze_expression(ctx, node->data.repeat_stmt.count);
            
            if (!type_is_numeric(count_type)) {
                symtab_error(ctx->symtab, node->loc,
                            "Repeat count must be numeric, got %s",
                            datatype_to_string(count_type));
                ctx->had_error = true;
            }
            
            symtab_enter_loop_scope(ctx->symtab);
            analyze_node(ctx, node->data.repeat_stmt.body);
            symtab_exit_scope(ctx->symtab);
            break;
        }
        
        case AST_FOR_EACH: {
            DataType iter_type = analyze_expression(ctx, node->data.for_each_stmt.iterable);
            
            if (iter_type != TYPE_LIST && iter_type != TYPE_TEXT &&
                iter_type != TYPE_UNKNOWN) {
                symtab_error(ctx->symtab, node->loc,
                            "Cannot iterate over %s (expected list or text)",
                            datatype_to_string(iter_type));
                ctx->had_error = true;
            }
            
            symtab_enter_loop_scope(ctx->symtab);
            
            /* Declare iterator variable */
            DataType elem_type = (iter_type == TYPE_TEXT) ? TYPE_TEXT : TYPE_UNKNOWN;
            const char *error = symtab_declare_variable(
                ctx->symtab,
                node->data.for_each_stmt.iterator_name,
                elem_type,
                false,
                node->loc
            );
            if (error) {
                symtab_error(ctx->symtab, node->loc, "%s", error);
                ctx->had_error = true;
            } else {
                Symbol *iter_sym = symtab_lookup(ctx->symtab, 
                                                 node->data.for_each_stmt.iterator_name);
                if (iter_sym) symtab_mark_initialized(iter_sym);
            }
            
            analyze_node(ctx, node->data.for_each_stmt.body);
            symtab_exit_scope(ctx->symtab);
            break;
        }
        
        case AST_RETURN: {
            if (!symtab_in_function(ctx->symtab)) {
                symtab_error(ctx->symtab, node->loc,
                            "'give back' (return) outside of function");
                ctx->had_error = true;
            } else {
                DataType expected = symtab_get_return_type(ctx->symtab);
                
                if (node->data.return_stmt.value) {
                    DataType actual = analyze_expression(ctx, node->data.return_stmt.value);
                    
                    if (expected == TYPE_NOTHING) {
                        symtab_error(ctx->symtab, node->loc,
                                    "Function should not return a value");
                        ctx->had_error = true;
                    } else if (!types_compatible(expected, actual)) {
                        symtab_error(ctx->symtab, node->loc,
                                    "Return type mismatch: expected %s, got %s",
                                    datatype_to_string(expected),
                                    datatype_to_string(actual));
                        ctx->had_error = true;
                    }
                } else {
                    if (expected != TYPE_NOTHING && expected != TYPE_UNKNOWN) {
                        symtab_error(ctx->symtab, node->loc,
                                    "Function should return %s",
                                    datatype_to_string(expected));
                        ctx->had_error = true;
                    }
                }
            }
            break;
        }
        
        case AST_BREAK: {
            if (!symtab_in_loop(ctx->symtab)) {
                symtab_error(ctx->symtab, node->loc,
                            "'stop' (break) outside of loop");
                ctx->had_error = true;
            }
            break;
        }
        
        case AST_CONTINUE: {
            if (!symtab_in_loop(ctx->symtab)) {
                symtab_error(ctx->symtab, node->loc,
                            "'skip' (continue) outside of loop");
                ctx->had_error = true;
            }
            break;
        }
        
        case AST_DISPLAY: {
            analyze_expression(ctx, node->data.display_stmt.value);
            break;
        }
        
        case AST_ASK: {
            if (node->data.ask_stmt.prompt) {
                analyze_expression(ctx, node->data.ask_stmt.prompt);
            }
            
            /* Check that target variable exists */
            if (node->data.ask_stmt.target_var) {
                Symbol *sym = symtab_lookup(ctx->symtab, node->data.ask_stmt.target_var);
                if (!sym) {
                    symtab_error(ctx->symtab, node->loc,
                                "Undefined variable '%s'", node->data.ask_stmt.target_var);
                    ctx->had_error = true;
                } else if (sym->kind == SYMBOL_CONSTANT) {
                    symtab_error(ctx->symtab, node->loc,
                                "Cannot read into constant '%s'", 
                                node->data.ask_stmt.target_var);
                    ctx->had_error = true;
                } else {
                    symtab_mark_initialized(sym);
                }
            }
            break;
        }
        
        case AST_READ: {
            if (node->data.read_stmt.target_var) {
                Symbol *sym = symtab_lookup(ctx->symtab, node->data.read_stmt.target_var);
                if (!sym) {
                    symtab_error(ctx->symtab, node->loc,
                                "Undefined variable '%s'", node->data.read_stmt.target_var);
                    ctx->had_error = true;
                } else if (sym->kind == SYMBOL_CONSTANT) {
                    symtab_error(ctx->symtab, node->loc,
                                "Cannot read into constant '%s'",
                                node->data.read_stmt.target_var);
                    ctx->had_error = true;
                } else {
                    symtab_mark_initialized(sym);
                }
            }
            break;
        }
        
        case AST_SECURE_ZONE: {
            symtab_enter_secure_scope(ctx->symtab);
            analyze_node(ctx, node->data.secure_zone.body);
            symtab_exit_scope(ctx->symtab);
            break;
        }
        
        case AST_EXPR_STMT: {
            analyze_expression(ctx, node->data.expr_stmt.expr);
            break;
        }
        
        default:
            break;
    }
}

/* ============================================================================
 * NODE ANALYSIS (dispatches to appropriate handler)
 * ============================================================================
 */

static void analyze_node(AnalyzerContext *ctx, ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_PROGRAM: {
            if (node->data.program.statements) {
                for (size_t i = 0; i < node->data.program.statements->count; i++) {
                    analyze_node(ctx, node->data.program.statements->nodes[i]);
                }
            }
            break;
        }
        
        case AST_BLOCK: {
            if (node->data.block.statements) {
                for (size_t i = 0; i < node->data.block.statements->count; i++) {
                    analyze_node(ctx, node->data.block.statements->nodes[i]);
                }
            }
            break;
        }
        
        default:
            analyze_statement(ctx, node);
            break;
    }
}

/* ============================================================================
 * MAIN ANALYSIS ENTRY POINT
 * ============================================================================
 */

SemanticResult semantic_analyze(ASTNode *program) {
    SemanticResult result = {0};
    
    if (!program) {
        result.success = false;
        result.error_count = 1;
        return result;
    }
    
    /* Create analyzer context */
    AnalyzerContext ctx = {0};
    ctx.symtab = symtab_create();
    ctx.had_error = false;
    
    /* Perform analysis */
    analyze_node(&ctx, program);
    
    /* Fill in result */
    result.success = !ctx.had_error && symtab_error_count(ctx.symtab) == 0;
    result.error_count = symtab_error_count(ctx.symtab);
    result.warning_count = symtab_warning_count(ctx.symtab);
    result.symtab = ctx.symtab;  /* Caller takes ownership */
    
    return result;
}

void semantic_result_free(SemanticResult *result) {
    if (result && result->symtab) {
        symtab_destroy(result->symtab);
        result->symtab = NULL;
    }
}
