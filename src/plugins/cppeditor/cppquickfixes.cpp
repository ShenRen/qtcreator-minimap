/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "cppeditor.h"
#include "cppquickfix.h"
#include "cppdeclfromdef.h"

#include <ASTVisitor.h>
#include <AST.h>
#include <ASTMatcher.h>
#include <ASTPatternBuilder.h>
#include <CoreTypes.h>
#include <Literals.h>
#include <Name.h>
#include <Names.h>
#include <Symbol.h>
#include <Symbols.h>
#include <Token.h>
#include <TranslationUnit.h>
#include <Type.h>

#include <cplusplus/DependencyTable.h>
#include <cplusplus/Overview.h>
#include <cplusplus/TypeOfExpression.h>
#include <cplusplus/CppRewriter.h>
#include <cpptools/cpptoolsconstants.h>
#include <extensionsystem/iplugin.h>

#include <QtGui/QApplication>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCursor>

using namespace CppEditor;
using namespace CppEditor::Internal;
using namespace TextEditor;
using namespace CPlusPlus;
using namespace Utils;

namespace {

/*
    Rewrite
    a op b -> !(a invop b)
    (a op b) -> !(a invop b)
    !(a op b) -> (a invob b)
*/
class UseInverseOp: public CppQuickFixFactory
{
public:
    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        QList<CppQuickFixOperation::Ptr> result;

        const QList<AST *> &path = state.path();
        int index = path.size() - 1;
        BinaryExpressionAST *binary = path.at(index)->asBinaryExpression();
        if (! binary)
            return result;
        if (! state.isCursorOn(binary->binary_op_token))
            return result;

        Kind invertToken;
        switch (state.tokenAt(binary->binary_op_token).kind()) {
        case T_LESS_EQUAL:
            invertToken = T_GREATER;
            break;
        case T_LESS:
            invertToken = T_GREATER_EQUAL;
            break;
        case T_GREATER:
            invertToken = T_LESS_EQUAL;
            break;
        case T_GREATER_EQUAL:
            invertToken = T_LESS;
            break;
        case T_EQUAL_EQUAL:
            invertToken = T_EXCLAIM_EQUAL;
            break;
        case T_EXCLAIM_EQUAL:
            invertToken = T_EQUAL_EQUAL;
            break;
        default:
            return result;
        }

        result.append(CppQuickFixOperation::Ptr(new Operation(state, index, binary, invertToken)));
        return result;
    }

private:
    class Operation: public CppQuickFixOperation
    {
        BinaryExpressionAST *binary;
        NestedExpressionAST *nested;
        UnaryExpressionAST *negation;

        QString replacement;

    public:
        Operation(const CppQuickFixState &state, int priority, BinaryExpressionAST *binary, Kind invertToken)
            : CppQuickFixOperation(state, priority)
            , binary(binary)
        {
            Token tok;
            tok.f.kind = invertToken;
            replacement = QLatin1String(tok.spell());

            // check for enclosing nested expression
            if (priority - 1 >= 0)
                nested = state.path()[priority - 1]->asNestedExpression();

            // check for ! before parentheses
            if (nested && priority - 2 >= 0) {
                negation = state.path()[priority - 2]->asUnaryExpression();
                if (negation && ! state.tokenAt(negation->unary_op_token).is(T_EXCLAIM))
                    negation = 0;
            }
        }

        virtual QString description() const
        {
            return QApplication::translate("CppTools::QuickFix", "Rewrite Using %1").arg(replacement);
        }

        virtual void createChanges()
        {
            ChangeSet changes;
            if (negation) {
                // can't remove parentheses since that might break precedence
                changes.remove(range(negation->unary_op_token));
            } else if (nested) {
                changes.insert(state().startOf(nested), "!");
            } else {
                changes.insert(state().startOf(binary), "!(");
                changes.insert(state().endOf(binary), ")");
            }
            changes.replace(range(binary->binary_op_token), replacement);
            refactoringChanges()->changeFile(fileName(), changes);
        }
    };
};

/*
    Rewrite
    a op b

    As
    b flipop a
*/
class FlipBinaryOp: public CppQuickFixFactory
{
public:
    virtual QList<QuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        QList<QuickFixOperation::Ptr> result;
        const QList<AST *> &path = state.path();

        int index = path.size() - 1;
        BinaryExpressionAST *binary = path.at(index)->asBinaryExpression();
        if (! binary)
            return result;
        if (! state.isCursorOn(binary->binary_op_token))
            return result;

        Kind flipToken;
        switch (state.tokenAt(binary->binary_op_token).kind()) {
        case T_LESS_EQUAL:
            flipToken = T_GREATER_EQUAL;
            break;
        case T_LESS:
            flipToken = T_GREATER;
            break;
        case T_GREATER:
            flipToken = T_LESS;
            break;
        case T_GREATER_EQUAL:
            flipToken = T_LESS_EQUAL;
            break;
        case T_EQUAL_EQUAL:
        case T_EXCLAIM_EQUAL:
        case T_AMPER_AMPER:
        case T_PIPE_PIPE:
            flipToken = T_EOF_SYMBOL;
            break;
        default:
            return result;
        }

        QString replacement;
        if (flipToken != T_EOF_SYMBOL) {
            Token tok;
            tok.f.kind = flipToken;
            replacement = QLatin1String(tok.spell());
        }

        result.append(QuickFixOperation::Ptr(new Operation(state, index, binary, replacement)));
        return result;
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state, int priority, BinaryExpressionAST *binary, QString replacement)
            : CppQuickFixOperation(state)
            , binary(binary)
            , replacement(replacement)
        {
            setPriority(priority);
        }

        virtual QString description() const
        {
            if (replacement.isEmpty())
                return QApplication::translate("CppTools::QuickFix", "Swap Operands");
            else
                return QApplication::translate("CppTools::QuickFix", "Rewrite Using %1").arg(replacement);
        }

        virtual void createChanges()
        {
            ChangeSet changes;

            changes.flip(range(binary->left_expression), range(binary->right_expression));
            if (! replacement.isEmpty())
                changes.replace(range(binary->binary_op_token), replacement);

            refactoringChanges()->changeFile(fileName(), changes);
        }

    private:
        BinaryExpressionAST *binary;
        QString replacement;
    };
};

/*
    Rewrite
    !a && !b

    As
    !(a || b)
*/
class RewriteLogicalAndOp: public CppQuickFixFactory
{
public:
    virtual QList<QuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        QList<QuickFixOperation::Ptr> result;
        BinaryExpressionAST *expression = 0;
        const QList<AST *> &path = state.path();

        int index = path.size() - 1;
        for (; index != -1; --index) {
            expression = path.at(index)->asBinaryExpression();
            if (expression)
                break;
        }

        if (! expression)
            return result;

        if (! state.isCursorOn(expression->binary_op_token))
            return result;

        QSharedPointer<Operation> op(new Operation(state));

        if (expression->match(op->pattern, &matcher) &&
                state.tokenAt(op->pattern->binary_op_token).is(T_AMPER_AMPER) &&
                state.tokenAt(op->left->unary_op_token).is(T_EXCLAIM) &&
                state.tokenAt(op->right->unary_op_token).is(T_EXCLAIM)) {
            op->setDescription(QApplication::translate("CppTools::QuickFix", "Rewrite Condition Using ||"));
            op->setPriority(index);
            result.append(op);
        }

        return result;
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        QSharedPointer<ASTPatternBuilder> mk;
        UnaryExpressionAST *left;
        UnaryExpressionAST *right;
        BinaryExpressionAST *pattern;

        Operation(const CppQuickFixState &state)
            : CppQuickFixOperation(state)
            , mk(new ASTPatternBuilder)
        {
            left = mk->UnaryExpression();
            right = mk->UnaryExpression();
            pattern = mk->BinaryExpression(left, right);
        }

        virtual void createChanges()
        {
            ChangeSet changes;
            changes.replace(range(pattern->binary_op_token), QLatin1String("||"));
            changes.remove(range(left->unary_op_token));
            changes.remove(range(right->unary_op_token));
            const int start = state().startOf(pattern);
            const int end = state().endOf(pattern);
            changes.insert(start, QLatin1String("!("));
            changes.insert(end, QLatin1String(")"));

            refactoringChanges()->changeFile(fileName(), changes);
            refactoringChanges()->reindent(fileName(), range(pattern));
        }
    };

private:
    ASTMatcher matcher;
};

class SplitSimpleDeclarationOp: public CppQuickFixFactory
{
    static bool checkDeclaration(SimpleDeclarationAST *declaration)
    {
        if (! declaration->semicolon_token)
            return false;

        if (! declaration->decl_specifier_list)
            return false;

        for (SpecifierListAST *it = declaration->decl_specifier_list; it; it = it->next) {
            SpecifierAST *specifier = it->value;

            if (specifier->asEnumSpecifier() != 0)
                return false;

            else if (specifier->asClassSpecifier() != 0)
                return false;
        }

        if (! declaration->declarator_list)
            return false;

        else if (! declaration->declarator_list->next)
            return false;

        return true;
    }

public:
    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        QList<CppQuickFixOperation::Ptr> result;
        CoreDeclaratorAST *core_declarator = 0;
        const QList<AST *> &path = state.path();

        for (int index = path.size() - 1; index != -1; --index) {
            AST *node = path.at(index);

            if (CoreDeclaratorAST *coreDecl = node->asCoreDeclarator())
                core_declarator = coreDecl;

            else if (SimpleDeclarationAST *simpleDecl = node->asSimpleDeclaration()) {
                if (checkDeclaration(simpleDecl)) {
                    SimpleDeclarationAST *declaration = simpleDecl;

                    const int cursorPosition = state.selectionStart();

                    const int startOfDeclSpecifier = state.startOf(declaration->decl_specifier_list->firstToken());
                    const int endOfDeclSpecifier = state.endOf(declaration->decl_specifier_list->lastToken() - 1);

                    if (cursorPosition >= startOfDeclSpecifier && cursorPosition <= endOfDeclSpecifier) {
                        // the AST node under cursor is a specifier.
                        return singleResult(new Operation(state, index, declaration));
                    }

                    if (core_declarator && state.isCursorOn(core_declarator)) {
                        // got a core-declarator under the text cursor.
                        return singleResult(new Operation(state, index, declaration));
                    }
                }

                break;
            }
        }

        return result;
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state, int priority, SimpleDeclarationAST *decl)
            : CppQuickFixOperation(state, priority)
            , declaration(decl)
        {
            setDescription(QApplication::translate("CppTools::QuickFix",
                                                   "Split Declaration"));
        }

        virtual void createChanges()
        {
            ChangeSet changes;

            SpecifierListAST *specifiers = declaration->decl_specifier_list;
            int declSpecifiersStart = state().startOf(specifiers->firstToken());
            int declSpecifiersEnd = state().endOf(specifiers->lastToken() - 1);
            int insertPos = state().endOf(declaration->semicolon_token);

            DeclaratorAST *prevDeclarator = declaration->declarator_list->value;

            for (DeclaratorListAST *it = declaration->declarator_list->next; it; it = it->next) {
                DeclaratorAST *declarator = it->value;

                changes.insert(insertPos, QLatin1String("\n"));
                changes.copy(declSpecifiersStart, declSpecifiersEnd, insertPos);
                changes.insert(insertPos, QLatin1String(" "));
                changes.move(range(declarator), insertPos);
                changes.insert(insertPos, QLatin1String(";"));

                const int prevDeclEnd = state().endOf(prevDeclarator);
                changes.remove(prevDeclEnd, state().startOf(declarator));

                prevDeclarator = declarator;
            }

            refactoringChanges()->changeFile(fileName(), changes);
            refactoringChanges()->reindent(fileName(), range(declaration));
        }

    private:
        SimpleDeclarationAST *declaration;
    };
};

/*
    Add curly braces to a if statement that doesn't already contain a
    compound statement.
*/
class AddBracesToIfOp: public CppQuickFixFactory
{
public:
    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        const QList<AST *> &path = state.path();

        // show when we're on the 'if' of an if statement
        int index = path.size() - 1;
        IfStatementAST *ifStatement = path.at(index)->asIfStatement();
        if (ifStatement && state.isCursorOn(ifStatement->if_token) && ifStatement->statement
            && ! ifStatement->statement->asCompoundStatement()) {
            return singleResult(new Operation(state, index, ifStatement->statement));
        }

        // or if we're on the statement contained in the if
        // ### This may not be such a good idea, consider nested ifs...
        for (; index != -1; --index) {
            IfStatementAST *ifStatement = path.at(index)->asIfStatement();
            if (ifStatement && ifStatement->statement
                && state.isCursorOn(ifStatement->statement)
                && ! ifStatement->statement->asCompoundStatement()) {
                return singleResult(new Operation(state, index, ifStatement->statement));
            }
        }

        // ### This could very well be extended to the else branch
        // and other nodes entirely.

        return noResult();
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state, int priority, StatementAST *statement)
            : CppQuickFixOperation(state, priority)
            , _statement(statement)
        {
            setDescription(QApplication::translate("CppTools::QuickFix",
                                                   "Add Curly Braces"));
        }

        virtual void createChanges()
        {
            ChangeSet changes;

            const int start = endOf(_statement->firstToken() - 1);
            changes.insert(start, QLatin1String(" {"));

            const int end = endOf(_statement->lastToken() - 1);
            changes.insert(end, "\n}");

            refactoringChanges()->changeFile(fileName(), changes);
            refactoringChanges()->reindent(fileName(), range(start, end));
        }

    private:
        StatementAST *_statement;
    };
};

/*
    Replace
    if (Type name = foo()) {...}

    With
    Type name = foo;
    if (name) {...}
*/
class MoveDeclarationOutOfIfOp: public CppQuickFixFactory
{
public:
    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        const QList<AST *> &path = state.path();
        QSharedPointer<Operation> op(new Operation(state));

        int index = path.size() - 1;
        for (; index != -1; --index) {
            if (IfStatementAST *statement = path.at(index)->asIfStatement()) {
                if (statement->match(op->pattern, &op->matcher) && op->condition->declarator) {
                    DeclaratorAST *declarator = op->condition->declarator;
                    op->core = declarator->core_declarator;
                    if (! op->core)
                        return noResult();

                    if (state.isCursorOn(op->core)) {
                        QList<CppQuickFixOperation::Ptr> result;
                        op->setPriority(index);
                        result.append(op);
                        return result;
                    }
                }
            }
        }

        return noResult();
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state)
            : CppQuickFixOperation(state)
        {
            setDescription(QApplication::translate("CppTools::QuickFix",
                                                   "Move Declaration out of Condition"));

            condition = mk.Condition();
            pattern = mk.IfStatement(condition);
        }

        virtual void createChanges()
        {
            ChangeSet changes;

            changes.copy(range(core), startOf(condition));

            int insertPos = startOf(pattern);
            changes.move(range(condition), insertPos);
            changes.insert(insertPos, QLatin1String(";\n"));

            refactoringChanges()->changeFile(fileName(), changes);
            refactoringChanges()->reindent(fileName(), range(pattern));
        }

        ASTMatcher matcher;
        ASTPatternBuilder mk;
        CPPEditor *editor;
        ConditionAST *condition;
        IfStatementAST *pattern;
        CoreDeclaratorAST *core;
    };
};

/*
    Replace
    while (Type name = foo()) {...}

    With
    Type name;
    while ((name = foo()) != 0) {...}
*/
class MoveDeclarationOutOfWhileOp: public CppQuickFixFactory
{
public:
    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        const QList<AST *> &path = state.path();
        QSharedPointer<Operation> op(new Operation(state));

        int index = path.size() - 1;
        for (; index != -1; --index) {
            if (WhileStatementAST *statement = path.at(index)->asWhileStatement()) {
                if (statement->match(op->pattern, &op->matcher) && op->condition->declarator) {
                    DeclaratorAST *declarator = op->condition->declarator;
                    op->core = declarator->core_declarator;

                    if (! op->core)
                        return noResult();

                    else if (! declarator->equals_token)
                        return noResult();

                    else if (! declarator->initializer)
                        return noResult();

                    if (state.isCursorOn(op->core)) {
                        QList<CppQuickFixOperation::Ptr> result;
                        op->setPriority(index);
                        result.append(op);
                        return result;
                    }
                }
            }
        }

        return noResult();
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state)
            : CppQuickFixOperation(state)
        {
            setDescription(QApplication::translate("CppTools::QuickFix",
                                                   "Move Declaration out of Condition"));

            condition = mk.Condition();
            pattern = mk.WhileStatement(condition);
        }

        virtual void createChanges()
        {
            ChangeSet changes;

            changes.insert(startOf(condition), QLatin1String("("));
            changes.insert(endOf(condition), QLatin1String(") != 0"));

            int insertPos = startOf(pattern);
            const int conditionStart = startOf(condition);
            changes.move(conditionStart, startOf(core), insertPos);
            changes.copy(range(core), insertPos);
            changes.insert(insertPos, QLatin1String(";\n"));

            refactoringChanges()->changeFile(fileName(), changes);
            refactoringChanges()->reindent(fileName(), range(pattern));
        }

        ASTMatcher matcher;
        ASTPatternBuilder mk;
        CPPEditor *editor;
        ConditionAST *condition;
        WhileStatementAST *pattern;
        CoreDeclaratorAST *core;
    };
};

/*
  Replace
     if (something && something_else) {
     }

  with
     if (something) {
        if (something_else) {
        }
     }

  and
    if (something || something_else)
      x;

  with
    if (something)
      x;
    else if (something_else)
      x;
*/
class SplitIfStatementOp: public CppQuickFixFactory
{
public:
    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        IfStatementAST *pattern = 0;
        const QList<AST *> &path = state.path();

        int index = path.size() - 1;
        for (; index != -1; --index) {
            AST *node = path.at(index);
            if (IfStatementAST *stmt = node->asIfStatement()) {
                pattern = stmt;
                break;
            }
        }

        if (! pattern || ! pattern->statement)
            return noResult();

        unsigned splitKind = 0;
        for (++index; index < path.size(); ++index) {
            AST *node = path.at(index);
            BinaryExpressionAST *condition = node->asBinaryExpression();
            if (! condition)
                return noResult();

            Token binaryToken = state.tokenAt(condition->binary_op_token);

            // only accept a chain of ||s or &&s - no mixing
            if (! splitKind) {
                splitKind = binaryToken.kind();
                if (splitKind != T_AMPER_AMPER && splitKind != T_PIPE_PIPE)
                    return noResult();
                // we can't reliably split &&s in ifs with an else branch
                if (splitKind == T_AMPER_AMPER && pattern->else_statement)
                    return noResult();
            } else if (splitKind != binaryToken.kind()) {
                return noResult();
            }

            if (state.isCursorOn(condition->binary_op_token))
                return singleResult(new Operation(state, index, pattern, condition));
        }

        return noResult();
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state, int priority,
                  IfStatementAST *pattern, BinaryExpressionAST *condition)
            : CppQuickFixOperation(state, priority)
            , pattern(pattern)
            , condition(condition)
        {
            setDescription(QApplication::translate("CppTools::QuickFix",
                                                   "Split if Statement"));
        }

        virtual void createChanges()
        {
            const Token binaryToken = state().tokenAt(condition->binary_op_token);

            if (binaryToken.is(T_AMPER_AMPER))
                splitAndCondition();
            else
                splitOrCondition();
        }

        void splitAndCondition()
        {
            ChangeSet changes;

            int startPos = startOf(pattern);
            changes.insert(startPos, QLatin1String("if ("));
            changes.move(range(condition->left_expression), startPos);
            changes.insert(startPos, QLatin1String(") {\n"));

            const int lExprEnd = endOf(condition->left_expression);
            changes.remove(lExprEnd, startOf(condition->right_expression));
            changes.insert(endOf(pattern), QLatin1String("\n}"));

            refactoringChanges()->changeFile(fileName(), changes);
            refactoringChanges()->reindent(fileName(), range(pattern));
        }

        void splitOrCondition()
        {
            ChangeSet changes;

            StatementAST *ifTrueStatement = pattern->statement;
            CompoundStatementAST *compoundStatement = ifTrueStatement->asCompoundStatement();

            int insertPos = endOf(ifTrueStatement);
            if (compoundStatement)
                changes.insert(insertPos, QLatin1String(" "));
            else
                changes.insert(insertPos, QLatin1String("\n"));
            changes.insert(insertPos, QLatin1String("else if ("));

            const int rExprStart = startOf(condition->right_expression);
            changes.move(rExprStart, startOf(pattern->rparen_token), insertPos);
            changes.insert(insertPos, QLatin1String(")"));

            const int rParenEnd = endOf(pattern->rparen_token);
            changes.copy(rParenEnd, endOf(pattern->statement), insertPos);

            const int lExprEnd = endOf(condition->left_expression);
            changes.remove(lExprEnd, startOf(condition->right_expression));

            refactoringChanges()->changeFile(fileName(), changes);
            refactoringChanges()->reindent(fileName(), range(pattern));
        }

    private:
        IfStatementAST *pattern;
        BinaryExpressionAST *condition;
    };
};

/*
  Replace
    "abcd"
  With
    QLatin1String("abcd")
*/
class WrapStringLiteral: public CppQuickFixFactory
{
public:
    enum Type { TypeString, TypeObjCString, TypeChar, TypeNone };

    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        ExpressionAST *literal = 0;
        Type type = TypeNone;
        const QList<AST *> &path = state.path();

        if (path.isEmpty())
            return noResult(); // nothing to do

        literal = path.last()->asStringLiteral();

        if (! literal) {
            literal = path.last()->asNumericLiteral();
            if (!literal || !state.tokenAt(literal->asNumericLiteral()->literal_token).is(T_CHAR_LITERAL))
                return noResult();
            else
                type = TypeChar;
        } else {
            type = TypeString;
        }

        if (path.size() > 1) {
            if (CallAST *call = path.at(path.size() - 2)->asCall()) {
                if (call->base_expression) {
                    if (SimpleNameAST *functionName = call->base_expression->asSimpleName()) {
                        const QByteArray id(state.tokenAt(functionName->identifier_token).identifier->chars());

                        if (id == "QT_TRANSLATE_NOOP" || id == "tr" || id == "trUtf8"
                                || (type == TypeString && (id == "QLatin1String" || id == "QLatin1Literal"))
                                || (type == TypeChar && id == "QLatin1Char"))
                            return noResult(); // skip it
                    }
                }
            }
        }

        if (type == TypeString) {
            if (state.charAt(state.startOf(literal)) == QLatin1Char('@'))
                type = TypeObjCString;
        }
        return singleResult(new Operation(state,
                                          path.size() - 1, // very high priority
                                          type,
                                          literal));
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state, int priority, Type type,
                  ExpressionAST *literal)
            : CppQuickFixOperation(state, priority)
            , type(type)
            , literal(literal)
        {
            if (type == TypeChar)
                setDescription(QApplication::translate("CppTools::QuickFix",
                                                       "Enclose in QLatin1Char(...)"));
            else
                setDescription(QApplication::translate("CppTools::QuickFix",
                                                       "Enclose in QLatin1String(...)"));
        }

        virtual void createChanges()
        {
            ChangeSet changes;

            const int startPos = startOf(literal);
            QLatin1String replacement = (type == TypeChar ? QLatin1String("QLatin1Char(")
                                                          : QLatin1String("QLatin1String("));

            if (type == TypeObjCString)
                changes.replace(startPos, startPos + 1, replacement);
            else
                changes.insert(startPos, replacement);

            changes.insert(endOf(literal), ")");

            refactoringChanges()->changeFile(fileName(), changes);
        }

    private:
        Type type;
        ExpressionAST *literal;
    };
};

/*
  Replace
    "abcd"
  With
    tr("abcd") or
    QCoreApplication::translate("CONTEXT", "abcd") or
    QT_TRANSLATE_NOOP("GLOBAL", "abcd")
*/
class TranslateStringLiteral: public CppQuickFixFactory
{
public:
    enum TranslationOption { unknown, useTr, useQCoreApplicationTranslate, useMacro };

    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        const QList<AST *> &path = state.path();
        // Initialize
        ExpressionAST *literal = 0;
        QString trContext;

        if (path.isEmpty())
            return noResult();

        literal = path.last()->asStringLiteral();
        if (!literal)
            return noResult(); // No string, nothing to do

        // Do we already have a translation markup?
        if (path.size() >= 2) {
            if (CallAST *call = path.at(path.size() - 2)->asCall()) {
                if (call->base_expression) {
                    if (SimpleNameAST *functionName = call->base_expression->asSimpleName()) {
                        const QByteArray id(state.tokenAt(functionName->identifier_token).identifier->chars());

                        if (id == "tr" || id == "trUtf8"
                                || id == "translate"
                                || id == "QT_TRANSLATE_NOOP"
                                || id == "QLatin1String" || id == "QLatin1Literal")
                            return noResult(); // skip it
                    }
                }
            }
        }

        QSharedPointer<Control> control = state.context().control();
        const Name *trName = control->nameId(control->findOrInsertIdentifier("tr"));

        // Check whether we are in a method:
        for (int i = path.size() - 1; i >= 0; --i)
        {
            if (FunctionDefinitionAST *definition = path.at(i)->asFunctionDefinition()) {
                Function *function = definition->symbol;
                ClassOrNamespace *b = state.context().lookupType(function);
                if (b) {
                    // Do we have a tr method?
                    foreach(const LookupItem &r, b->find(trName)) {
                        Symbol *s = r.declaration();
                        if (s->type()->isFunctionType()) {
                            // no context required for tr
                            return singleResult(new Operation(state, path.size() - 1, literal, useTr, trContext));
                        }
                    }
                }
                // We need to do a QCA::translate, so we need a context.
                // Use fully qualified class name:
                Overview oo;
                foreach (const Name *n, LookupContext::path(function)) {
                    if (!trContext.isEmpty())
                        trContext.append(QLatin1String("::"));
                    trContext.append(oo.prettyName(n));
                }
                // ... or global if none available!
                if (trContext.isEmpty())
                    trContext = QLatin1String("GLOBAL");
                return singleResult(new Operation(state, path.size() - 1, literal, useQCoreApplicationTranslate, trContext));
            }
        }

        // We need to use Q_TRANSLATE_NOOP
        return singleResult(new Operation(state, path.size() - 1, literal, useMacro, QLatin1String("GLOBAL")));
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state, int priority, ExpressionAST *literal, TranslationOption option, const QString &context)
            : CppQuickFixOperation(state, priority)
            , m_literal(literal)
            , m_option(option)
            , m_context(context)
        {
            setDescription(QApplication::translate("CppTools::QuickFix", "Mark as translateable"));
        }

        virtual void createChanges()
        {
            ChangeSet changes;

            const int startPos = startOf(m_literal);
            QString replacement(QLatin1String("tr("));
            if (m_option == useQCoreApplicationTranslate) {
                replacement = QLatin1String("QCoreApplication::translate(\"")
                        + m_context + QLatin1String("\", ");
            } else if (m_option == useMacro) {
                replacement = QLatin1String("QT_TRANSLATE_NOOP(\"")
                        + m_context + QLatin1String("\", ");
            }

            changes.insert(startPos, replacement);
            changes.insert(endOf(m_literal), QLatin1String(")"));

            refactoringChanges()->changeFile(fileName(), changes);
        }

    private:
        ExpressionAST *m_literal;
        TranslationOption m_option;
        QString m_context;
    };
};

class CStringToNSString: public CppQuickFixFactory
{
public:
    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        if (state.editor()->mimeType() != CppTools::Constants::OBJECTIVE_CPP_SOURCE_MIMETYPE)
            return noResult();

        StringLiteralAST *stringLiteral = 0;
        CallAST *qlatin1Call = 0;
        const QList<AST *> &path = state.path();

        if (path.isEmpty())
            return noResult(); // nothing to do

        stringLiteral = path.last()->asStringLiteral();

        if (! stringLiteral)
            return noResult();

        else if (state.charAt(state.startOf(stringLiteral)) == QLatin1Char('@'))
            return noResult(); // it's already an objc string literal.

        else if (path.size() > 1) {
            if (CallAST *call = path.at(path.size() - 2)->asCall()) {
                if (call->base_expression) {
                    if (SimpleNameAST *functionName = call->base_expression->asSimpleName()) {
                        const QByteArray id(state.tokenAt(functionName->identifier_token).identifier->chars());

                        if (id == "QLatin1String" || id == "QLatin1Literal")
                            qlatin1Call = call;
                    }
                }
            }
        }

        return singleResult(new Operation(state, path.size() - 1, stringLiteral, qlatin1Call));
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state, int priority, StringLiteralAST *stringLiteral, CallAST *qlatin1Call)
            : CppQuickFixOperation(state, priority)
            , stringLiteral(stringLiteral)
            , qlatin1Call(qlatin1Call)
        {
            setDescription(QApplication::translate("CppTools::QuickFix",
                                                   "Convert to Objective-C String Literal"));
        }

        virtual void createChanges()
        {
            ChangeSet changes;

            if (qlatin1Call) {
                changes.replace(startOf(qlatin1Call), startOf(stringLiteral), QLatin1String("@"));
                changes.remove(endOf(stringLiteral), endOf(qlatin1Call));
            } else {
                changes.insert(startOf(stringLiteral), "@");
            }

            refactoringChanges()->changeFile(fileName(), changes);
        }

    private:
        StringLiteralAST *stringLiteral;
        CallAST *qlatin1Call;
    };
};

/*
  Base class for converting numeric literals between decimal, octal and hex.
  Does the base check for the specific ones and parses the number.
  Test cases:
    0xFA0Bu;
    0X856A;
    298.3;
    199;
    074;
    199L;
    074L;
    -199;
    -017;
    0783; // invalid octal
    0; // border case, allow only hex<->decimal
*/
class ConvertNumericLiteral: public CppQuickFixFactory
{
public:
    virtual QList<QuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        QList<QuickFixOperation::Ptr> result;

        const QList<AST *> &path = state.path();

        if (path.isEmpty())
            return result; // nothing to do

        NumericLiteralAST *literal = path.last()->asNumericLiteral();

        if (! literal)
            return result;

        Token token = state.tokenAt(literal->asNumericLiteral()->literal_token);
        if (!token.is(T_NUMERIC_LITERAL))
            return result;
        const NumericLiteral *numeric = token.number;
        if (numeric->isDouble() || numeric->isFloat())
            return result;

        // remove trailing L or U and stuff
        const char * const spell = numeric->chars();
        int numberLength = numeric->size();
        while (numberLength > 0 && (spell[numberLength-1] < '0' || spell[numberLength-1] > 'F'))
            --numberLength;
        if (numberLength < 1)
            return result;

        // convert to number
        bool valid;
        ulong value = QString::fromUtf8(spell).left(numberLength).toULong(&valid, 0);
        if (!valid) // e.g. octal with digit > 7
            return result;

        const int priority = path.size() - 1; // very high priority
        const int start = state.startOf(literal);
        const char * const str = numeric->chars();

        if (!numeric->isHex()) {
            /*
              Convert integer literal to hex representation.
              Replace
                32
                040
              With
                0x20

            */
            QString replacement;
            replacement.sprintf("0x%lX", value);
            QuickFixOperation::Ptr op(new ConvertNumeric(state, start, start + numberLength, replacement));
            op->setDescription(QApplication::translate("CppTools::QuickFix", "Convert to Hexadecimal"));
            op->setPriority(priority);
            result.append(op);
        }

        if (value != 0) {
            if (!(numberLength > 1 && str[0] == '0' && str[1] != 'x' && str[1] != 'X')) {
                /*
                  Convert integer literal to octal representation.
                  Replace
                    32
                    0x20
                  With
                    040
                */
                QString replacement;
                replacement.sprintf("0%lo", value);
                QuickFixOperation::Ptr op(new ConvertNumeric(state, start, start + numberLength, replacement));
                op->setDescription(QApplication::translate("CppTools::QuickFix", "Convert to Octal"));
                op->setPriority(priority);
                result.append(op);
            }
        }

        if (value != 0 || numeric->isHex()) {
            if (!(numberLength > 1 && str[0] != '0')) {
                /*
                  Convert integer literal to decimal representation.
                  Replace
                    0x20
                    040
                  With
                    32
                */
                QString replacement;
                replacement.sprintf("%lu", value);
                QuickFixOperation::Ptr op(new ConvertNumeric(state, start, start + numberLength, replacement));
                op->setDescription(QApplication::translate("CppTools::QuickFix", "Convert to Decimal"));
                op->setPriority(priority);
                result.append(op);
            }
        }

        return result;
    }

private:
    class ConvertNumeric: public CppQuickFixOperation
    {
    public:
        ConvertNumeric(const CppQuickFixState &state, int start, int end, const QString &replacement)
            : CppQuickFixOperation(state)
            , start(start)
            , end(end)
            , replacement(replacement)
        {}

        virtual void createChanges()
        {
            ChangeSet changes;
            changes.replace(start, end, replacement);
            refactoringChanges()->changeFile(fileName(), changes);
        }

    protected:
        int start, end;
        QString replacement;
    };
};

/*
  Adds missing case statements for "switch (enumVariable)"
*/
class CompleteSwitchCaseStatement: public CppQuickFixFactory
{
public:
    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        const QList<AST *> &path = state.path();

        if (path.isEmpty())
            return noResult(); // nothing to do

        // look for switch statement
        for (int depth = path.size() - 1; depth >= 0; --depth) {
            AST *ast = path.at(depth);
            SwitchStatementAST *switchStatement = ast->asSwitchStatement();
            if (switchStatement) {
                if (!state.isCursorOn(switchStatement->switch_token) || !switchStatement->statement)
                    return noResult();
                CompoundStatementAST *compoundStatement = switchStatement->statement->asCompoundStatement();
                if (!compoundStatement) // we ignore pathologic case "switch (t) case A: ;"
                    return noResult();
                // look if the condition's type is an enum
                if (Enum *e = conditionEnum(state, switchStatement)) {
                    // check the possible enum values
                    QStringList values;
                    Overview prettyPrint;
                    for (unsigned i = 0; i < e->memberCount(); ++i) {
                        if (Declaration *decl = e->memberAt(i)->asDeclaration()) {
                            values << prettyPrint(LookupContext::fullyQualifiedName(decl));
                        }
                    }
                    // Get the used values
                    Block *block = switchStatement->symbol;
                    CaseStatementCollector caseValues(state.document(), state.snapshot(),
                        state.document()->scopeAt(block->line(), block->column()));
                    QStringList usedValues = caseValues(switchStatement);
                    // save the values that would be added
                    foreach (const QString &usedValue, usedValues)
                        values.removeAll(usedValue);
                    if (values.isEmpty())
                        return noResult();
                    return singleResult(new Operation(state, depth, compoundStatement, values));
                }
                return noResult();
            }
        }

        return noResult();
    }

protected:
    Enum *conditionEnum(const CppQuickFixState &state, SwitchStatementAST *statement)
    {
        Block *block = statement->symbol;
        Scope *scope = state.document()->scopeAt(block->line(), block->column());
        TypeOfExpression typeOfExpression;
        typeOfExpression.init(state.document(), state.snapshot());
        const QList<LookupItem> results = typeOfExpression(statement->condition,
                                                           state.document(),
                                                           scope);
        foreach (LookupItem result, results) {
            FullySpecifiedType fst = result.type();
            if (Enum *e = result.declaration()->type()->asEnumType())
                return e;
            if (NamedType *namedType = fst->asNamedType()) {
                QList<LookupItem> candidates =
                        typeOfExpression.context().lookup(namedType->name(), scope);
                foreach (const LookupItem &r, candidates) {
                    Symbol *candidate = r.declaration();
                    if (Enum *e = candidate->asEnum()) {
                        return e;
                    }
                }
            }
        }
        return 0;
    }

    class CaseStatementCollector : public ASTVisitor
    {
    public:
        CaseStatementCollector(Document::Ptr document, const Snapshot &snapshot,
                               Scope *scope)
            : ASTVisitor(document->translationUnit()),
            document(document),
            scope(scope)
        {
            typeOfExpression.init(document, snapshot);
        }

        QStringList operator ()(AST *ast)
        {
            values.clear();
            foundCaseStatementLevel = false;
            accept(ast);
            return values;
        }

        bool preVisit(AST *ast) {
            if (CaseStatementAST *cs = ast->asCaseStatement()) {
                foundCaseStatementLevel = true;
                ExpressionAST *expression = cs->expression->asSimpleName();
                if (!expression)
                    expression = cs->expression->asQualifiedName();
                if (expression) {
                    LookupItem item = typeOfExpression(expression,
                                                       document,
                                                       scope).first();
                    values << prettyPrint(LookupContext::fullyQualifiedName(item.declaration()));
                }
                return true;
            } else if (foundCaseStatementLevel) {
                return false;
            }
            return true;
        }

        Overview prettyPrint;
        bool foundCaseStatementLevel;
        QStringList values;
        TypeOfExpression typeOfExpression;
        Document::Ptr document;
        Scope *scope;
    };

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state, int priority, CompoundStatementAST *compoundStatement, const QStringList &values)
            : CppQuickFixOperation(state, priority)
            , compoundStatement(compoundStatement)
            , values(values)
        {
            setDescription(QApplication::translate("CppTools::QuickFix",
                                                   "Complete Switch Statement"));
        }


        virtual void createChanges()
        {
            ChangeSet changes;
            int start = endOf(compoundStatement->lbrace_token);
            changes.insert(start, QLatin1String("\ncase ")
                           + values.join(QLatin1String(":\nbreak;\ncase "))
                           + QLatin1String(":\nbreak;"));
            refactoringChanges()->changeFile(fileName(), changes);
            refactoringChanges()->reindent(fileName(), range(compoundStatement));
        }

        CompoundStatementAST *compoundStatement;
        QStringList values;
    };
};


class FixForwardDeclarationOp: public CppQuickFixFactory
{
public:
    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        const QList<AST *> &path = state.path();

        for (int index = path.size() - 1; index != -1; --index) {
            AST *ast = path.at(index);
            if (NamedTypeSpecifierAST *namedTy = ast->asNamedTypeSpecifier()) {
                if (Symbol *fwdClass = checkName(state, namedTy->name))
                    return singleResult(new Operation(state, index, fwdClass));
            } else if (ElaboratedTypeSpecifierAST *eTy = ast->asElaboratedTypeSpecifier()) {
                if (Symbol *fwdClass = checkName(state, eTy->name))
                    return singleResult(new Operation(state, index, fwdClass));
            }
        }

        return noResult();
    }

protected:
    static Symbol *checkName(const CppQuickFixState &state, NameAST *ast)
    {
        if (ast && state.isCursorOn(ast)) {
            if (const Name *name = ast->name) {
                unsigned line, column;
                state.document()->translationUnit()->getTokenStartPosition(ast->firstToken(), &line, &column);

                Symbol *fwdClass = 0;

                foreach (const LookupItem &r, state.context().lookup(name, state.document()->scopeAt(line, column))) {
                    if (! r.declaration())
                        continue;
                    else if (ForwardClassDeclaration *fwd = r.declaration()->asForwardClassDeclaration())
                        fwdClass = fwd;
                    else if (r.declaration()->isClass())
                        return 0; // nothing to do.
                }

                return fwdClass;
            }
        }

        return 0;
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state, int priority, Symbol *fwdClass)
            : CppQuickFixOperation(state, priority)
            , fwdClass(fwdClass)
        {
            setDescription(QApplication::translate("CppTools::QuickFix",
                                                   "#include header file"));
        }

        virtual void createChanges()
        {
            Q_ASSERT(fwdClass != 0);

            if (Class *k = state().snapshot().findMatchingClassDeclaration(fwdClass)) {
                const QString headerFile = QString::fromUtf8(k->fileName(), k->fileNameLength());

                // collect the fwd headers
                Snapshot fwdHeaders;
                fwdHeaders.insert(state().snapshot().document(headerFile));
                foreach (Document::Ptr doc, state().snapshot()) {
                    QFileInfo headerFileInfo(doc->fileName());
                    if (doc->globalSymbolCount() == 0 && doc->includes().size() == 1)
                        fwdHeaders.insert(doc);
                    else if (headerFileInfo.suffix().isEmpty())
                        fwdHeaders.insert(doc);
                }


                DependencyTable dep;
                dep.build(fwdHeaders);
                QStringList candidates = dep.dependencyTable().value(headerFile);

                const QString className = QString::fromUtf8(k->identifier()->chars());

                QString best;
                foreach (const QString &c, candidates) {
                    QFileInfo headerFileInfo(c);
                    if (headerFileInfo.fileName() == className) {
                        best = c;
                        break;
                    } else if (headerFileInfo.fileName().at(0).isUpper()) {
                        best = c;
                        // and continue
                    } else if (! best.isEmpty()) {
                        if (c.count(QLatin1Char('/')) < best.count(QLatin1Char('/')))
                            best = c;
                    }
                }

                if (best.isEmpty())
                    best = headerFile;

                int pos = startOf(1);

                unsigned currentLine = state().textCursor().blockNumber() + 1;
                unsigned bestLine = 0;
                foreach (const Document::Include &incl, state().document()->includes()) {
                    if (incl.line() < currentLine)
                        bestLine = incl.line();
                }

                if (bestLine)
                    pos = state().editor()->document()->findBlockByNumber(bestLine).position();

                Utils::ChangeSet changes;
                changes.insert(pos, QString("#include <%1>\n").arg(QFileInfo(best).fileName()));
                refactoringChanges()->changeFile(fileName(), changes);
            }
        }

    private:
        Symbol *fwdClass;
    };
};

class AddLocalDeclarationOp: public CppQuickFixFactory
{
public:
    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        const QList<AST *> &path = state.path();

        for (int index = path.size() - 1; index != -1; --index) {
            if (BinaryExpressionAST *binary = path.at(index)->asBinaryExpression()) {
                if (binary->left_expression && binary->right_expression && state.tokenAt(binary->binary_op_token).is(T_EQUAL)) {
                    if (state.isCursorOn(binary->left_expression) && binary->left_expression->asSimpleName() != 0) {
                        SimpleNameAST *nameAST = binary->left_expression->asSimpleName();
                        const QList<LookupItem> results = state.context().lookup(nameAST->name, state.scopeAt(nameAST->firstToken()));
                        Declaration *decl = 0;
                        foreach (const LookupItem &r, results) {
                            if (! r.declaration())
                                continue;
                            else if (Declaration *d = r.declaration()->asDeclaration()) {
                                if (! d->type()->isFunctionType()) {
                                    decl = d;
                                    break;
                                }
                            }
                        }

                        if (! decl) {
                            return singleResult(new Operation(state, index, binary));
                        }
                    }
                }
            }
        }

        return noResult();
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state, int priority, BinaryExpressionAST *binaryAST)
            : CppQuickFixOperation(state, priority)
            , binaryAST(binaryAST)
        {
            setDescription(QApplication::translate("CppTools::QuickFix", "Add local declaration"));
        }

        virtual void createChanges()
        {
            TypeOfExpression typeOfExpression;
            typeOfExpression.init(state().document(), state().snapshot(), state().context().bindings());
            const QList<LookupItem> result = typeOfExpression(state().textOf(binaryAST->right_expression),
                                                              state().scopeAt(binaryAST->firstToken()),
                                                              TypeOfExpression::Preprocess);

            if (! result.isEmpty()) {

                SubstitutionEnvironment env;
                env.setContext(state().context());
                env.switchScope(result.first().scope());
                UseQualifiedNames q;
                env.enter(&q);

                Control *control = state().context().control().data();
                FullySpecifiedType tn = rewriteType(result.first().type(), &env, control);

                Overview oo;
                QString ty = oo(tn);
                if (! ty.isEmpty()) {
                    const QChar ch = ty.at(ty.size() - 1);

                    if (ch.isLetterOrNumber() || ch == QLatin1Char(' ') || ch == QLatin1Char('>'))
                        ty += QLatin1Char(' ');

                    Utils::ChangeSet changes;
                    changes.insert(startOf(binaryAST), ty);
                    refactoringChanges()->changeFile(fileName(), changes);
                }
            }
        }

    private:
        BinaryExpressionAST *binaryAST;
    };
};

/*
 * Turns "an_example_symbol" into "anExampleSymbol" and
 * "AN_EXAMPLE_SYMBOL" into "AnExampleSymbol".
 */
class ToCamelCaseConverter : public CppQuickFixFactory
{
public:
    virtual QList<CppQuickFixOperation::Ptr> match(const CppQuickFixState &state)
    {
        const QList<AST *> &path = state.path();

        if (path.isEmpty())
            return noResult();

        AST * const ast = path.last();
        const Name *name = 0;
        if (const NameAST * const nameAst = ast->asName()) {
            if (nameAst->name && nameAst->name->asNameId())
                name = nameAst->name;
        } else if (const NamespaceAST * const namespaceAst = ast->asNamespace()) {
            name = namespaceAst->symbol->name();
        }

        if (!name)
            return noResult();

        QString newName = QString::fromUtf8(name->identifier()->chars());
        if (newName.length() < 3)
            return noResult();
        for (int i = 1; i < newName.length() - 1; ++i) {
            if (Operation::isConvertibleUnderscore(newName, i))
                return singleResult(new Operation(state, path.size() - 1, newName));
        }

        return noResult();
    }

private:
    class Operation: public CppQuickFixOperation
    {
    public:
        Operation(const CppQuickFixState &state, int priority, const QString &newName)
            : CppQuickFixOperation(state, priority)
            , m_name(newName)
        {
            setDescription(QApplication::translate("CppTools::QuickFix",
                                                   "Convert to Camel Case ..."));
        }

        virtual void createChanges()
        {
            for (int i = 1; i < m_name.length(); ++i) {
                QCharRef c = m_name[i];
                if (c.isUpper()) {
                    c = c.toLower();
                } else if (i < m_name.length() - 1
                           && isConvertibleUnderscore(m_name, i)) {
                    m_name.remove(i, 1);
                    m_name[i] = m_name.at(i).toUpper();
                }
            }
            static_cast<CppEditor::Internal::CPPEditor*>(state().editor())->renameUsagesNow(m_name);
        }

        static bool isConvertibleUnderscore(const QString &name, int pos)
        {
            return name.at(pos) == QLatin1Char('_') && name.at(pos+1).isLetter()
                    && !(pos == 1 && name.at(0) == QLatin1Char('m'));
        }

    private:
        QString m_name;
    };
};

} // end of anonymous namespace

void CppQuickFixCollector::registerQuickFixes(ExtensionSystem::IPlugin *plugIn)
{
    plugIn->addAutoReleasedObject(new UseInverseOp);
    plugIn->addAutoReleasedObject(new FlipBinaryOp);
    plugIn->addAutoReleasedObject(new RewriteLogicalAndOp);
    plugIn->addAutoReleasedObject(new SplitSimpleDeclarationOp);
    plugIn->addAutoReleasedObject(new AddBracesToIfOp);
    plugIn->addAutoReleasedObject(new MoveDeclarationOutOfIfOp);
    plugIn->addAutoReleasedObject(new MoveDeclarationOutOfWhileOp);
    plugIn->addAutoReleasedObject(new SplitIfStatementOp);
    plugIn->addAutoReleasedObject(new WrapStringLiteral);
    plugIn->addAutoReleasedObject(new TranslateStringLiteral);
    plugIn->addAutoReleasedObject(new CStringToNSString);
    plugIn->addAutoReleasedObject(new ConvertNumericLiteral);
    plugIn->addAutoReleasedObject(new CompleteSwitchCaseStatement);
    plugIn->addAutoReleasedObject(new FixForwardDeclarationOp);
    plugIn->addAutoReleasedObject(new AddLocalDeclarationOp);
    plugIn->addAutoReleasedObject(new ToCamelCaseConverter);
    plugIn->addAutoReleasedObject(new Internal::DeclFromDef);
}