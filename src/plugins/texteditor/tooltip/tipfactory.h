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

#ifndef TIPFACTORY_H
#define TIPFACTORY_H

#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE
class QTipLabel;
QT_END_NAMESPACE

namespace TextEditor {

class TipContent;

namespace Internal {

class TipFactory
{
public:
    TipFactory();
    virtual ~TipFactory();
    Q_DISABLE_COPY(TipFactory)

    virtual QTipLabel *createTip(const TipContent &content, QWidget *w);
};

} // namespace Internal
} // namespace TextEditor

#endif // TIPFACTORY_H