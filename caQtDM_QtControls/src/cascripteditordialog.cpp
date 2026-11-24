/*
 *  This file is part of the caQtDM Framework, developed at the Paul Scherrer Institut,
 *  Villigen, Switzerland
 *
 *  The caQtDM Framework is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  The caQtDM Framework is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the caQtDM Framework.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (c) 2026
 *
 *  Author:
 *    Julian Houba
 *  Contact details:
 *    julian.houba@psi.ch
 */

#include "cascripteditordialog.h"
#include "cascript.h"

#include <QDesignerFormWindowCursorInterface>
#include <QDesignerFormWindowInterface>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJSEngine>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTimer>
#include <QVBoxLayout>

namespace {

class ScriptEditor : public QPlainTextEdit
{
    class Gutter : public QWidget
    {
    public:
        explicit Gutter(ScriptEditor *editor) : QWidget(editor), m_editor(editor) {}

    protected:
        void paintEvent(QPaintEvent *) override { m_editor->paintLineNumbers(this); }

    private:
        ScriptEditor *m_editor;
    };

public:
    explicit ScriptEditor(QWidget *parent) : QPlainTextEdit(parent), m_gutter(this)
    {
        setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        setLineWrapMode(QPlainTextEdit::NoWrap);
        connect(this, &QPlainTextEdit::blockCountChanged, this, &ScriptEditor::layoutGutter);
        connect(this, &QPlainTextEdit::updateRequest, &m_gutter, [this](const QRect &, int delta) {
            if (delta) m_gutter.scroll(0, delta);
            else m_gutter.update();
        });
        layoutGutter();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QPlainTextEdit::resizeEvent(event);
        layoutGutter();
    }

private:
    void layoutGutter()
    {
        const int width = fontMetrics().horizontalAdvance(QString::number(blockCount())) + 12;
        setViewportMargins(width, 0, 0, 0);
        const QRect area = contentsRect();
        m_gutter.setGeometry(area.left(), area.top(), width, viewport()->height());
    }

    void paintLineNumbers(QWidget *gutter)
    {
        QPainter painter(gutter);
        painter.fillRect(gutter->rect(), palette().alternateBase());
        painter.setPen(palette().text().color());
        for (QTextBlock block = firstVisibleBlock(); block.isValid(); block = block.next()) {
            const QRectF geometry = blockBoundingGeometry(block).translated(contentOffset());
            if (geometry.top() >= gutter->height()) break;
            if (!block.isVisible() || geometry.bottom() < 0) continue;
            painter.drawText(QRectF(0, geometry.top(), gutter->width() - 6, geometry.height()),
                             Qt::AlignRight | Qt::AlignTop, QString::number(block.blockNumber() + 1));
        }
    }

    Gutter m_gutter;
};

class JavaScriptHighlighter : public QSyntaxHighlighter
{
public:
    explicit JavaScriptHighlighter(QPlainTextEdit *editor)
        : QSyntaxHighlighter(editor->document()), m_editor(editor) {}

protected:
    void highlightBlock(const QString &text) override
    {
        static const QRegularExpression tokens(QStringLiteral(
            R"js(//|/\*|["'`]|[\p{L}_$][\p{L}\p{M}\p{N}_$]*|\b(?:0[xX][\da-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+|\d[\d_]*(?:\.[\d_]*)?(?:[eE][+-]?[\d_]+)?)n?)js"));
        static const QRegularExpression keywords(QStringLiteral(
            "^(?:async|await|break|case|catch|class|const|continue|debugger|default|delete|do|else|export|extends|"
            "false|finally|for|function|if|import|in|instanceof|let|new|null|of|return|static|super|switch|"
            "this|throw|true|try|typeof|undefined|var|void|while|with|yield)$"));

        const bool dark = m_editor->palette().base().color().lightnessF() < 0.5;
        const QColor comment = dark ? QColor(156, 188, 133) : QColor(56, 107, 46);
        const QColor string = dark ? QColor(245, 174, 137) : QColor(150, 55, 25);
        const QColor keyword = dark ? QColor(154, 195, 255) : QColor(41, 75, 155);
        const QColor number = dark ? QColor(219, 173, 246) : QColor(115, 55, 142);

        int state = qMax(int(Code), previousBlockState());
        int position = 0;
        while (position < text.size() || state != Code) {
            if (state != Code) {
                position = highlightQuoted(text, position, position, state, comment, string);
                if (state != Code) break;
                continue;
            }
            const auto token = tokens.match(text, position);
            if (!token.hasMatch()) break;
            const QString value = token.captured();
            const int start = token.capturedStart();
            position = token.capturedEnd();
            if (value == QStringLiteral("//")) {
                setFormat(start, text.size() - start, comment);
                break;
            }
            if (value == QStringLiteral("/*")) state = BlockComment;
            else if (value == QStringLiteral("'")) state = SingleQuote;
            else if (value == QStringLiteral("\"")) state = DoubleQuote;
            else if (value == QStringLiteral("`")) state = Template;
            else if (value.at(0).isDigit()) setFormat(start, value.size(), number);
            else if (keywords.match(value).hasMatch()) setFormat(start, value.size(), keyword);

            if (state != Code) {
                position = highlightQuoted(text, start, position, state, comment, string);
                if (state != Code) break;
            }
        }
        setCurrentBlockState(state);
    }

private:
    enum State { Code, BlockComment, SingleQuote, DoubleQuote, Template };

    int highlightQuoted(const QString &text, int start, int position, int &state,
                        const QColor &comment, const QColor &string)
    {
        if (state == BlockComment) {
            const int closing = text.indexOf(QStringLiteral("*/"), position);
            const int end = closing >= 0 ? closing + 2 : int(text.size());
            setFormat(start, end - start, comment);
            if (closing >= 0) state = Code;
            return end;
        }

        const QChar quote = QLatin1Char(state == SingleQuote ? '\'' : state == DoubleQuote ? '"' : '`');
        bool continued = false;
        while (position < text.size()) {
            const QChar ch = text.at(position++);
            if (ch == QLatin1Char('\\')) {
                if (position < text.size()) ++position;
                else continued = true;
            } else if (ch == quote) {
                state = Code;
                break;
            }
        }
        setFormat(start, position - start, string);
        if (state != Template && !continued) state = Code;
        return position;
    }

    QPlainTextEdit *m_editor;
};

} // namespace

CaScriptEditorDialog::CaScriptEditorDialog(caScript *widget, QWidget *parent)
    : QDialog(parent), m_widget(widget), m_editor(new ScriptEditor(this)),
      m_diagnostic(new QLabel(this)), m_checkTimer(new QTimer(this))
{
    setWindowTitle(tr("Edit JavaScript"));
    resize(720, 480);

    m_editor->setObjectName(QStringLiteral("scriptEditor"));
    m_editor->setPlainText(widget->getScript());
    m_initialText = source();
    new JavaScriptHighlighter(m_editor);

    m_diagnostic->setObjectName(QStringLiteral("scriptDiagnostic"));
    m_diagnostic->setTextFormat(Qt::PlainText);
    m_diagnostic->setWordWrap(true);
    m_diagnostic->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *check = new QPushButton(tr("Check Syntax"), this);
    check->setObjectName(QStringLiteral("checkScriptSyntax"));
    check->setAutoDefault(false);
    auto *validation = new QHBoxLayout;
    validation->addWidget(check);
    validation->addWidget(m_diagnostic, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_editor, 1);
    layout->addLayout(validation);
    layout->addWidget(buttons);

    m_checkTimer->setSingleShot(true);
    m_checkTimer->setInterval(300);
    connect(m_checkTimer, &QTimer::timeout, this, &CaScriptEditorDialog::checkSyntax);
    connect(check, &QPushButton::clicked, this, &CaScriptEditorDialog::checkSyntax);
    connect(buttons, &QDialogButtonBox::accepted, this, &CaScriptEditorDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() {
        m_editor->setExtraSelections({});
        m_diagnostic->clear();
        m_checkTimer->start();
    });
    checkSyntax();
    m_editor->setFocus();
}

QString CaScriptEditorDialog::source() const
{
    // The raw text preserves non-breaking spaces inside JavaScript strings.
    return m_editor->document()->toRawText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
}

bool CaScriptEditorDialog::checkSyntax()
{
    m_checkTimer->stop();
    m_editor->setExtraSelections({});

    QJSEngine engine;
    QJSValue constructor = engine.globalObject().property(QStringLiteral("Function"));
    const QJSValue result = constructor.callAsConstructor({
        QJSValue(QStringLiteral("inputs")), QJSValue(QStringLiteral("outputs")),
        QJSValue(QStringLiteral("console")), QJSValue(QStringLiteral("EpicsType")),
        QJSValue(source())
    });
    if (result.isCallable()) {
        m_diagnostic->setText(tr("JavaScript syntax is valid."));
        return true;
    }

    const QString message = result.isError() ? result.toString() : tr("JavaScript syntax check failed.");
    // Function adds one source line before the body on Qt 5 and Qt 6.
    const int line = result.property(QStringLiteral("lineNumber")).toInt() - 1;
    if (line < 1 || line > m_editor->blockCount()) {
        m_diagnostic->setText(message);
        return false;
    }

    m_diagnostic->setText(tr("Line %1: %2").arg(line).arg(message));
    QTextEdit::ExtraSelection selection;
    selection.cursor = QTextCursor(m_editor->document()->findBlockByNumber(line - 1));
    selection.cursor.select(QTextCursor::LineUnderCursor);
    selection.format.setBackground(QColor(211, 47, 47, 55));
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.format.setUnderlineColor(QColor(211, 47, 47));
    selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    m_editor->setExtraSelections({selection});
    return false;
}

void CaScriptEditorDialog::accept()
{
    if (!checkSyntax()) return;

    auto *form = m_widget ? QDesignerFormWindowInterface::findFormWindow(m_widget) : nullptr;
    if (!form || !form->cursor()) {
        m_diagnostic->setText(tr("This editor requires a Qt Designer form."));
        return;
    }

    const QString script = source();
    if (script != m_initialText) {
        form->cursor()->setWidgetProperty(m_widget, QStringLiteral("script"), script);
    }
    QDialog::accept();
}
