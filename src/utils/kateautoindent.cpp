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
    return QString();
}

bool KateAutoIndent::doIndent(int line, int indentDepth, int align)
{
    return false;
}

bool KateAutoIndent::doIndentRelative(int line, int change)
{
    return false;
}

void KateAutoIndent::keepIndent(int line)
{
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

