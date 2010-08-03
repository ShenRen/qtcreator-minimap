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

#include "crumblepath.h"
#include "stylehelper.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>
#include <QResizeEvent>
#include <QPainter>
#include <QImage>

namespace Utils {

static const int ArrowBorderSize = 12;

class CrumblePathButton : public QPushButton
{
public:
    enum SegmentType {
        LastSegment = 1,
        MiddleSegment = 2,
        FirstSegment = 4
    };

    explicit CrumblePathButton(const QString &title, QWidget *parent = 0);
    void setSegmentType(int type);
protected:
    void paintEvent(QPaintEvent *);
    void mouseMoveEvent(QMouseEvent *e);
    void leaveEvent(QEvent *);
    void mousePressEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);

private:
    void tintImages();

private:
    bool m_isHovering;
    bool m_isPressed;
    bool m_isEnd;
    QColor m_baseColor;
    QImage m_segment;
    QImage m_segmentEnd;
    QImage m_segmentSelected;
    QImage m_segmentSelectedEnd;
    QImage m_segmentHover;
    QImage m_segmentHoverEnd;
    QPoint m_textPos;
};

CrumblePathButton::CrumblePathButton(const QString &title, QWidget *parent)
    : QPushButton(title, parent), m_isHovering(false), m_isPressed(false), m_isEnd(true)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    setToolTip(title);
    setMinimumHeight(24);
    setMaximumHeight(24);
    setMouseTracking(true);
    m_textPos.setX(18);
    m_textPos.setY(height());
    m_baseColor = StyleHelper::baseColor();

    m_segment = QImage(":/utils/images/crumblepath-segment.png");
    m_segmentSelected = QImage(":/utils/images/crumblepath-segment-selected.png");
    m_segmentHover = QImage(":/utils/images/crumblepath-segment-hover.png");
    m_segmentEnd = QImage(":/utils/images/crumblepath-segment-end.png");
    m_segmentSelectedEnd = QImage(":/utils/images/crumblepath-segment-selected-end.png");
    m_segmentHoverEnd = QImage(":/utils/images/crumblepath-segment-hover-end.png");

    tintImages();
}

void CrumblePathButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QRect geom(0, 0, geometry().width(), geometry().height());

    if (StyleHelper::baseColor() != m_baseColor) {
        m_baseColor = StyleHelper::baseColor();
        tintImages();
    }

    if (m_isEnd) {
        if (m_isPressed) {
            Utils::StyleHelper::drawCornerImage(m_segmentSelectedEnd, &p, geom, 2, 0, 2, 0);
        } else if (m_isHovering) {
            Utils::StyleHelper::drawCornerImage(m_segmentHoverEnd, &p, geom, 2, 0, 2, 0);
        } else {
            Utils::StyleHelper::drawCornerImage(m_segmentEnd, &p, geom, 2, 0, 2, 0);
        }
    } else {
        if (m_isPressed) {
            Utils::StyleHelper::drawCornerImage(m_segmentSelected, &p, geom, 2, 0, 12, 0);
        } else if (m_isHovering) {
            Utils::StyleHelper::drawCornerImage(m_segmentHover, &p, geom, 2, 0, 12, 0);
        } else {
            Utils::StyleHelper::drawCornerImage(m_segment, &p, geom, 2, 0, 12, 0);
        }
    }
    p.setPen(StyleHelper::panelTextColor());
    QFontMetrics fm(p.font());
    QString textToDraw = fm.elidedText(text(), Qt::ElideRight, geom.width() - m_textPos.x());

    p.drawText(QRectF(m_textPos.x(), 4, geom.width(), geom.height()), textToDraw);
}

void CrumblePathButton::tintImages()
{
    StyleHelper::tintImage(m_segmentEnd, m_baseColor);
    StyleHelper::tintImage(m_segmentSelectedEnd, m_baseColor);
    StyleHelper::tintImage(m_segmentHoverEnd, m_baseColor);
    StyleHelper::tintImage(m_segmentSelected, m_baseColor);
    StyleHelper::tintImage(m_segmentHover, m_baseColor);
    StyleHelper::tintImage(m_segment, m_baseColor);
}

void CrumblePathButton::leaveEvent(QEvent *e)
{
    QPushButton::leaveEvent(e);
    m_isHovering = false;
    update();
}

void CrumblePathButton::mouseMoveEvent(QMouseEvent *e)
{
    QPushButton::mouseMoveEvent(e);
    m_isHovering = true;
    update();
}

void CrumblePathButton::mousePressEvent(QMouseEvent *e)
{
    QPushButton::mousePressEvent(e);
    m_isPressed = true;
    update();
}

void CrumblePathButton::mouseReleaseEvent(QMouseEvent *e)
{
    QPushButton::mouseReleaseEvent(e);
    m_isPressed = false;
    update();
}

void CrumblePathButton::setSegmentType(int type)
{
    bool useLeftPadding = !(type & FirstSegment);
    m_isEnd = (type & LastSegment);
    m_textPos.setX(useLeftPadding ? 18 : 4);
}

//
// CrumblePath
//
CrumblePath::CrumblePath(QWidget *parent) :
    QWidget(parent), m_background(new QWidget(this))
{
    m_baseColor = StyleHelper::baseColor();

    setMinimumHeight(25);
    setMaximumHeight(25);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);

    setBackgroundStyle();
    m_background->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
}

CrumblePath::~CrumblePath()
{
    qDeleteAll(m_buttons);
    m_buttons.clear();
}

void CrumblePath::setBackgroundStyle()
{
    m_background->setStyleSheet("QWidget { background-color:" + m_baseColor.name() + ";}");
}

void CrumblePath::pushElement(const QString &title)
{
    CrumblePathButton *newButton = new CrumblePathButton(title, this);
    newButton->hide();
    connect(newButton, SIGNAL(clicked()), SLOT(mapClickToIndex()));
    connect(newButton, SIGNAL(customContextMenuRequested(QPoint)), SLOT(mapContextMenuRequestToIndex()));

    int segType = CrumblePathButton::MiddleSegment;
    if (!m_buttons.isEmpty()) {
        if (m_buttons.length() == 1)
            segType = segType | CrumblePathButton::FirstSegment;
        m_buttons.last()->setSegmentType(segType);
    } else {
        segType = CrumblePathButton::FirstSegment | CrumblePathButton::LastSegment;
        newButton->setSegmentType(segType);
    }
    m_buttons.append(newButton);

    resizeButtons();
}

void CrumblePath::popElement()
{
    QWidget *last = m_buttons.last();
    m_buttons.removeLast();
    last->setParent(0);
    last->deleteLater();

    int segType = CrumblePathButton::MiddleSegment | CrumblePathButton::LastSegment;
    if (!m_buttons.isEmpty()) {
        if (m_buttons.length() == 1)
            segType = CrumblePathButton::FirstSegment | CrumblePathButton::LastSegment;
        m_buttons.last()->setSegmentType(segType);
    }
    resizeButtons();
}

void CrumblePath::clear()
{
    while (!m_buttons.isEmpty()) {
        popElement();
    }
}

void CrumblePath::resizeEvent(QResizeEvent *)
{
    resizeButtons();
}

void CrumblePath::resizeButtons()
{
    int buttonMinWidth = 0;
    int buttonMaxWidth = 0;
    int totalWidthLeft = width();

    if (m_buttons.length() >= 1) {
        QPoint nextElementPosition(0,0);

        m_buttons[0]->raise();
        // rearrange all items so that the first item is on top (added last).
        for(int i = 0; i < m_buttons.length() ; ++i) {
            CrumblePathButton *button = m_buttons[i];

            QFontMetrics fm(button->font());
            buttonMinWidth = ArrowBorderSize + fm.width(button->text()) + ArrowBorderSize * 2 ;
            buttonMaxWidth = (totalWidthLeft + ArrowBorderSize * (m_buttons.length() - i)) / (m_buttons.length() - i);

            if (buttonMinWidth > buttonMaxWidth && i < m_buttons.length() - 1) {
                buttonMinWidth = buttonMaxWidth;
            } else if (i > 3 && (i == m_buttons.length() - 1)) {
                buttonMinWidth = width() - nextElementPosition.x();
                buttonMaxWidth = buttonMinWidth;
            }

            button->setMinimumWidth(buttonMinWidth);
            button->setMaximumWidth(buttonMaxWidth);
            button->move(nextElementPosition);

            nextElementPosition.rx() += button->width() - ArrowBorderSize;
            totalWidthLeft -= button->width();

            button->show();
            if (i > 0)
                button->stackUnder(m_buttons[i - 1]);
        }

    }

    m_background->setGeometry(0,0, width(), height());
    m_background->update();
}

void CrumblePath::mapClickToIndex()
{
    QObject *element = sender();
    for (int i = 0; i < m_buttons.length(); ++i) {
        if (m_buttons[i] == element) {
            emit elementClicked(i);
            return;
        }
    }
}

void CrumblePath::mapContextMenuRequestToIndex()
{
    QObject *element = sender();
    for (int i = 0; i < m_buttons.length(); ++i) {
        if (m_buttons[i] == element) {
            emit elementContextMenuRequested(i);
            return;
        }
    }
}

void CrumblePath::paintEvent(QPaintEvent *event)
{
    if (StyleHelper::baseColor() != m_baseColor) {
        m_baseColor = StyleHelper::baseColor();
        setBackgroundStyle();
    }

    QWidget::paintEvent(event);
}

} // namespace QmlViewer