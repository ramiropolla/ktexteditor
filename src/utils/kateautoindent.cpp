/* This file is part of the KDE libraries
   Copyright (C) 2003 Jesse Yurkovich <yurkjes@iit.edu>
   Copyright (C) 2004 >Anders Lund <anders@alweb.dk> (KateVarIndent class)
   Copyright (C) 2005 Dominik Haumann <dhdev@gmx.de> (basic support for config page)

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kateautoindent.h"

#include "kateconfig.h"
#include "katehighlight.h"
#include "kateglobal.h"
#include "kateindentscript.h"
#include "katescriptmanager.h"
#include "kateview.h"
#include "kateextendedattribute.h"
#include "katedocument.h"
#include "katepartdebug.h"

#include <KLocalizedString>

#include <cctype>

namespace {
inline const QString MODE_NONE()
{
    return QStringLiteral("none");
}
}

//BEGIN KateAutoIndent

QStringList KateAutoIndent::listModes()
{
    return QStringList(i18nc("Autoindent mode", "None"));
}

QStringList KateAutoIndent::listIdentifiers()
{
    return QStringList(MODE_NONE());
}

int KateAutoIndent::modeCount()
{
    return 1;
}

QString KateAutoIndent::modeName(int mode)
{
    return MODE_NONE();
}

QString KateAutoIndent::modeDescription(int mode)
{
    return i18nc("Autoindent mode", "None");
}

QString KateAutoIndent::modeRequiredStyle(int mode)
{
    return QString();
}

uint KateAutoIndent::modeNumber(const QString &name)
{
    return 0;
}

KateAutoIndent::KateAutoIndent(KTextEditor::DocumentPrivate *_doc)
    : QObject(_doc), doc(_doc), m_script(nullptr)
{
    // don't call updateConfig() here, document might is not ready for that....

    // on script reload, the script pointer is invalid -> force reload
    connect(KTextEditor::EditorPrivate::self()->scriptManager(), SIGNAL(reloaded()),
            this, SLOT(reloadScript()));
}

KateAutoIndent::~KateAutoIndent()
{
}

QString KateAutoIndent::tabString(int length, int align) const
{
    QString s;
    length = qMin(length, 256);  // sanity check for large values of pos
    int spaces = qBound(0, align - length, 256);

    if (!useSpaces) {
        s.append(QString(length / tabWidth, QLatin1Char('\t')));
        length = length % tabWidth;
    }
    s.append(QString(length + spaces, QLatin1Char(' ')));

    return s;
}

bool KateAutoIndent::doIndent(int line, int indentDepth, int align)
{
    Kate::TextLine textline = doc->plainKateTextLine(line);

    // textline not found, cu
    if (!textline) {
        return false;
    }

    // sanity check
    if (indentDepth < 0) {
        indentDepth = 0;
    }

    const QString oldIndentation = textline->leadingWhitespace();

    // Preserve existing "tabs then spaces" alignment if and only if:
    //  - no alignment was passed to doIndent and
    //  - we aren't using spaces for indentation and
    //  - we aren't rounding indentation up to the next multiple of the indentation width and
    //  - we aren't using a combination to tabs and spaces for alignment, or in other words
    //    the indent width is a multiple of the tab width.
    bool preserveAlignment = !useSpaces && keepExtra && indentWidth % tabWidth == 0;
    if (align == 0 && preserveAlignment) {
        // Count the number of consecutive spaces at the end of the existing indentation
        int i = oldIndentation.size() - 1;
        while (i >= 0 && oldIndentation.at(i) == QLatin1Char(' ')) {
            --i;
        }
        // Use the passed indentDepth as the alignment, and set the indentDepth to
        // that value minus the number of spaces found (but don't let it get negative).
        align = indentDepth;
        indentDepth = qMax(0, align - (oldIndentation.size() - 1 - i));
    }

    QString indentString = tabString(indentDepth, align);

    // Modify the document *ONLY* if smth has really changed!
    if (oldIndentation != indentString) {
        // remove leading whitespace, then insert the leading indentation
        doc->editStart();
        doc->editRemoveText(line, 0, oldIndentation.length());
        doc->editInsertText(line, 0, indentString);
        doc->editEnd();
    }

    return true;
}

bool KateAutoIndent::doIndentRelative(int line, int change)
{
    Kate::TextLine textline = doc->plainKateTextLine(line);

    // get indent width of current line
    int indentDepth = textline->indentDepth(tabWidth);
    int extraSpaces = indentDepth % indentWidth;

    // add change
    indentDepth += change;

    // if keepExtra is off, snap to a multiple of the indentWidth
    if (!keepExtra && extraSpaces > 0) {
        if (change < 0) {
            indentDepth += indentWidth - extraSpaces;
        } else {
            indentDepth -= extraSpaces;
        }
    }

    // do indent
    return doIndent(line, indentDepth);
}

void KateAutoIndent::keepIndent(int line)
{
    // no line in front, no work...
    if (line <= 0) {
        return;
    }

    // keep indentation: find line with content
    int nonEmptyLine = line - 1;
    while (nonEmptyLine >= 0) {
        if (doc->lineLength(nonEmptyLine) > 0) {
            break;
        }
        --nonEmptyLine;
    }
    Kate::TextLine prevTextLine = doc->plainKateTextLine(nonEmptyLine);
    Kate::TextLine textLine     = doc->plainKateTextLine(line);

    // textline not found, cu
    if (!prevTextLine || !textLine) {
        return;
    }

    const QString previousWhitespace = prevTextLine->leadingWhitespace();

    // remove leading whitespace, then insert the leading indentation
    doc->editStart();

    if (!keepExtra) {
        const QString currentWhitespace = textLine->leadingWhitespace();
        doc->editRemoveText(line, 0, currentWhitespace.length());
    }

    doc->editInsertText(line, 0, previousWhitespace);
    doc->editEnd();
}

void KateAutoIndent::reloadScript()
{
    // small trick to force reload
    m_script = nullptr; // prevent dangling pointer
    QString currentMode = m_mode;
    m_mode = QString();
    setMode(currentMode);
}

void KateAutoIndent::scriptIndent(KTextEditor::ViewPrivate *view, const KTextEditor::Cursor &position, QChar typedChar)
{
}

bool KateAutoIndent::isStyleProvided(const KateIndentScript *script, const KateHighlighting *highlight)
{
    return true;
}

void KateAutoIndent::setMode(const QString &name)
{
    m_script = nullptr;
    m_mode = MODE_NONE();
}

void KateAutoIndent::checkRequiredStyle()
{
}

void KateAutoIndent::updateConfig()
{
    KateDocumentConfig *config = doc->config();

    useSpaces   = config->replaceTabsDyn();
    keepExtra   = config->keepExtraSpaces();
    tabWidth    = config->tabWidth();
    indentWidth = config->indentationWidth();
}

bool KateAutoIndent::changeIndent(const KTextEditor::Range &range, int change)
{
    QList<int> skippedLines;

    // loop over all lines given...
    for (int line = range.start().line() < 0 ? 0 : range.start().line();
            line <= qMin(range.end().line(), doc->lines() - 1); ++line) {
        // don't indent empty lines
        if (doc->line(line).isEmpty()) {
            skippedLines.append(line);
            continue;
        }
        // don't indent the last line when the cursor is on the first column
        if (line == range.end().line() && range.end().column() == 0) {
            skippedLines.append(line);
            continue;
        }

        doIndentRelative(line, change * indentWidth);
    }

    if (skippedLines.count() > range.numberOfLines()) {
        // all lines were empty, so indent them nevertheless
        foreach (int line, skippedLines) {
            doIndentRelative(line, change * indentWidth);
        }
    }

    return true;
}

void KateAutoIndent::indent(KTextEditor::ViewPrivate *view, const KTextEditor::Range &range)
{
}

void KateAutoIndent::userTypedChar(KTextEditor::ViewPrivate *view, const KTextEditor::Cursor &position, QChar typedChar)
{
}
//END KateAutoIndent

//BEGIN KateViewIndentAction
KateViewIndentationAction::KateViewIndentationAction(KTextEditor::DocumentPrivate *_doc, const QString &text, QObject *parent)
    : KActionMenu(text, parent), doc(_doc)
{
    connect(menu(), SIGNAL(aboutToShow()), this, SLOT(slotAboutToShow()));
    actionGroup = new QActionGroup(menu());
}

void KateViewIndentationAction::slotAboutToShow()
{
    QStringList modes = KateAutoIndent::listModes();

    menu()->clear();
    foreach (QAction *action, actionGroup->actions()) {
        actionGroup->removeAction(action);
    }
    for (int z = 0; z < modes.size(); ++z) {
        QAction *action = menu()->addAction(QLatin1Char('&') + KateAutoIndent::modeDescription(z).replace(QLatin1Char('&'), QLatin1String("&&")));
        actionGroup->addAction(action);
        action->setCheckable(true);
        action->setData(z);

        QString requiredStyle = KateAutoIndent::modeRequiredStyle(z);
        action->setEnabled(requiredStyle.isEmpty() || requiredStyle == doc->highlight()->style());

        if (doc->config()->indentationMode() == KateAutoIndent::modeName(z)) {
            action->setChecked(true);
        }
    }

    disconnect(menu(), SIGNAL(triggered(QAction*)), this, SLOT(setMode(QAction*)));
    connect(menu(), SIGNAL(triggered(QAction*)), this, SLOT(setMode(QAction*)));
}

void KateViewIndentationAction::setMode(QAction *action)
{
    // set new mode
    doc->config()->setIndentationMode(KateAutoIndent::modeName(action->data().toInt()));
    doc->rememberUserDidSetIndentationMode();
}
//END KateViewIndentationAction

