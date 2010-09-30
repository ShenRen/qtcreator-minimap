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

#ifndef INSERTIONPOINTLOCATOR_H
#define INSERTIONPOINTLOCATOR_H

#include "cpptools_global.h"

#include <ASTfwd.h>
#include <CPlusPlusForwardDeclarations.h>
#include <Symbols.h>

#include <cplusplus/CppDocument.h>

namespace CppTools {

class CppRefactoringChanges;

class CPPTOOLS_EXPORT InsertionLocation
{
public:
    InsertionLocation();
    InsertionLocation(const QString &fileName, const QString &prefix,
                      const QString &suffix, unsigned line, unsigned column);

    QString fileName() const
    { return m_fileName; }

    /// \returns The prefix to insert before any other text.
    QString prefix() const
    { return m_prefix; }

    /// \returns The suffix to insert after the other inserted text.
    QString suffix() const
    { return m_suffix; }

    /// \returns The line where to insert. The line number is 1-based.
    unsigned line() const
    { return m_line; }

    /// \returns The column where to insert. The column number is 1-based.
    unsigned column() const
    { return m_column; }

    bool isValid() const
    { return !m_fileName.isEmpty() && m_line > 0 && m_column > 0; }

private:
    QString m_fileName;
    QString m_prefix;
    QString m_suffix;
    unsigned m_line;
    unsigned m_column;
};

class CPPTOOLS_EXPORT InsertionPointLocator
{
public:
    enum AccessSpec {
        Invalid = -1,
        Signals = 0,

        Public = 1,
        Protected = 2,
        Private = 3,

        SlotBit = 1 << 2,

        PublicSlot    = Public    | SlotBit,
        ProtectedSlot = Protected | SlotBit,
        PrivateSlot   = Private   | SlotBit,
    };

public:
    InsertionPointLocator(CppRefactoringChanges *refactoringChanges);

    InsertionLocation methodDeclarationInClass(const QString &fileName,
                                               const CPlusPlus::Class *clazz,
                                               AccessSpec xsSpec) const;

    QList<InsertionLocation> methodDefinition(CPlusPlus::Declaration *declaration) const;

private:
    CppRefactoringChanges *m_refactoringChanges;
};

} // namespace CppTools

#endif // INSERTIONPOINTLOCATOR_H