#include <QtTest>
#include <QObject>
#include <QList>
#include <QTextDocument>
#include <QTextBlock>

#include <qmljseditor/qmljseditorcodeformatter.h>

using namespace QmlJSEditor;

class tst_QMLCodeFormatter: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void objectDefinitions1();
    void objectDefinitions2();
    void expressionEndSimple();
    void expressionEnd();
    void expressionEndBracket();
    void expressionEndParen();
    void objectBinding();
    void arrayBinding();
    void functionDeclaration();
    void functionExpression();
    void propertyDeclarations();
    void signalDeclarations();
    void ifBinding1();
    void ifBinding2();
    void ifStatementWithoutBraces1();
    void ifStatementWithoutBraces2();
    void ifStatementWithBraces1();
    void ifStatementWithBraces2();
    void ifStatementMixed();
    void ifStatementAndComments();
    void ifStatementLongCondition();
    void moreIfThenElse();
    void strayElse();
    void oneLineIf();
    void forStatement();
    void whileStatement();
    void tryStatement();
    void doWhile();
    void cStyleComments();
    void cppStyleComments();
    void qmlKeywords();
    void ternary();
    void switch1();
//    void gnuStyle();
//    void whitesmithsStyle();
    void expressionContinuation();
};

struct Line {
    Line(QString l)
        : line(l)
    {
        for (int i = 0; i < l.size(); ++i) {
            if (!l.at(i).isSpace()) {
                expectedIndent = i;
                return;
            }
        }
        expectedIndent = l.size();
    }

    Line(QString l, int expect)
        : line(l), expectedIndent(expect)
    {}

    QString line;
    int expectedIndent;
};

QString concatLines(QList<Line> lines)
{
    QString result;
    foreach (const Line &l, lines) {
        result += l.line;
        result += "\n";
    }
    return result;
}

void checkIndent(QList<Line> data, int style = 0)
{
    Q_UNUSED(style)

    QString text = concatLines(data);
    QTextDocument document(text);
    QtStyleCodeFormatter formatter;

    int i = 0;
    foreach (const Line &l, data) {
        QTextBlock b = document.findBlockByLineNumber(i);
        if (l.expectedIndent != -1) {
            int actualIndent = formatter.indentFor(b);
            if (actualIndent != l.expectedIndent) {
                QFAIL(QString("Wrong indent in line %1 with text '%2', expected indent %3, got %4").arg(
                        QString::number(i+1), l.line, QString::number(l.expectedIndent), QString::number(actualIndent)).toLatin1().constData());
            }
        }
        formatter.updateLineStateChange(b);
        ++i;
    }
}

void tst_QMLCodeFormatter::objectDefinitions1()
{
    QList<Line> data;
    data << Line("import Qt 4.7")
         << Line("")
         << Line("Rectangle {")
         << Line("    foo: bar;")
         << Line("    Item {")
         << Line("        x: 42;")
         << Line("        y: x;")
         << Line("    }")
         << Line("    Component.onCompleted: foo;")
         << Line("    ")
         << Line("    Foo.Bar {")
         << Line("        width: 12 + 54;")
         << Line("        anchors.fill: parent;")
         << Line("    }")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::objectDefinitions2()
{
    QList<Line> data;
    data << Line("import Qt 4.7")
         << Line("")
         << Line("Rectangle {")
         << Line("    foo: bar;")
         << Line("    Image { source: \"a+b+c\"; x: 42; y: 12 }")
         << Line("    Component.onCompleted: foo;")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::expressionEndSimple()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    foo: bar +")
         << Line("         foo(4, 5) +")
         << Line("         7")
         << Line("    x: 42")
         << Line("    y: 43")
         << Line("    width: 10")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::expressionEnd()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    foo: bar +")
         << Line("         foo(4")
         << Line("             + 5)")
         << Line("         + 7")
         << Line("    x: 42")
         << Line("       + 43")
         << Line("       + 10")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::expressionEndParen()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    foo: bar")
         << Line("         (foo(4")
         << Line("              + 5)")
         << Line("          + 7,")
         << Line("          abc)")
         << Line("    x: a + b(fpp, ba + 12) + foo(")
         << Line("           bar,")
         << Line("           10)")
         << Line("       + 10")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::expressionEndBracket()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    foo: bar")
         << Line("         [foo[4")
         << Line("              + 5]")
         << Line("          + 7,")
         << Line("          abc]")
         << Line("    x: a + b[fpp, ba + 12] + foo[")
         << Line("           bar,")
         << Line("           10]")
         << Line("       + 10")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::objectBinding()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    foo: bar")
         << Line("    x: 3")
         << Line("    foo: Gradient {")
         << Line("        x: 12")
         << Line("        y: x")
         << Line("    }")
         << Line("    Item {")
         << Line("        states: State {}")
         << Line("    }")
         << Line("    x: 1")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::arrayBinding()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    foo: bar")
         << Line("    x: 3")
         << Line("    foo: [")
         << Line("        State {")
         << Line("            y: x")
         << Line("        },")
         << Line("        State")
         << Line("        {")
         << Line("        }")
         << Line("    ]")
         << Line("    foo: [")
         << Line("        1 +")
         << Line("        2")
         << Line("        + 345 * foo(")
         << Line("            bar, car,")
         << Line("            dar),")
         << Line("        x, y,")
         << Line("        z,")
         << Line("    ]")
         << Line("    x: 1")
         << Line("}")
         ;
    checkIndent(data);
}



void tst_QMLCodeFormatter::moreIfThenElse()
{
    QList<Line> data;
    data << Line("Image {")
         << Line("    source: {")
         << Line("        if(type == 1) {")
         << Line("            \"pics/blueStone.png\";")
         << Line("        } else if (type == 2) {")
         << Line("            \"pics/head.png\";")
         << Line("        } else {")
         << Line("            \"pics/redStone.png\";")
         << Line("        }")
         << Line("    }")
         << Line("}");
    checkIndent(data);
}


void tst_QMLCodeFormatter::functionDeclaration()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    foo: bar")
         << Line("    function foo(a, b, c) {")
         << Line("        if (a)")
         << Line("            b;")
         << Line("    }")
         << Line("    property alias boo :")
         << Line("        foo")
         << Line("    Item {")
         << Line("        property variant g : Gradient {")
         << Line("            v: 12")
         << Line("        }")
         << Line("        function bar(")
         << Line("            a, b,")
         << Line("            c)")
         << Line("        {")
         << Line("            var b")
         << Line("        }")
         << Line("        function bar(a,")
         << Line("                     a, b,")
         << Line("                     c)")
         << Line("        {")
         << Line("            var b")
         << Line("        }")
         << Line("    }")
         << Line("    x: 1")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::functionExpression()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("onFoo: {", 4)
         << Line("    function foo(a, b, c) {")
         << Line("        if (a)")
         << Line("            b;")
         << Line("    }")
         << Line("    return function(a, b) { return a + b; }")
         << Line("    return function foo(a, b) {")
         << Line("        return a")
         << Line("    }")
         << Line("}")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::propertyDeclarations()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    foo: bar")
         << Line("    property int foo : 2 +")
         << Line("                       x")
         << Line("    property list<Foo> bar")
         << Line("    property alias boo :")
         << Line("        foo")
         << Line("    Item {")
         << Line("        property variant g : Gradient {")
         << Line("            v: 12")
         << Line("        }")
         << Line("        default property Item g")
         << Line("        default property Item g : parent.foo")
         << Line("    }")
         << Line("    x: 1")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::signalDeclarations()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    foo: bar")
         << Line("    signal foo")
         << Line("    x: bar")
         << Line("    signal bar(a, int b)")
         << Line("    signal bar2()")
         << Line("    Item {")
         << Line("        signal property")
         << Line("        signal import(a, b);")
         << Line("    }")
         << Line("    x: 1")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::ifBinding1()
{
    QList<Line> data;
    data << Line("A.Rectangle {")
         << Line("    foo: bar")
         << Line("    x: if (a) b")
         << Line("    x: if (a)")
         << Line("           b")
         << Line("    x: if (a) b;")
         << Line("    x: if (a)")
         << Line("           b;")
         << Line("    x: if (a) b; else c")
         << Line("    x: if (a) b")
         << Line("       else c")
         << Line("    x: if (a) b;")
         << Line("       else c")
         << Line("    x: if (a) b;")
         << Line("       else")
         << Line("           c")
         << Line("    x: if (a)")
         << Line("           b")
         << Line("       else")
         << Line("           c")
         << Line("    x: if (a) b; else c;")
         << Line("    x: 1")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::ifBinding2()
{
    QList<Line> data;
    data << Line("A.Rectangle {")
         << Line("    foo: bar")
         << Line("    x: if (a) b +")
         << Line("              5 +")
         << Line("              5 * foo(")
         << Line("                  1, 2)")
         << Line("       else a =")
         << Line("            foo(15,")
         << Line("                bar(")
         << Line("                    1),")
         << Line("                bar)")
         << Line("    x: if (a) b")
         << Line("              + 5")
         << Line("              + 5")
         << Line("    x: if (a)")
         << Line("           b")
         << Line("               + 5")
         << Line("               + 5")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::ifStatementWithoutBraces1()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    x: if (a)")
         << Line("           if (b)")
         << Line("               foo")
         << Line("           else if (c)")
         << Line("               foo")
         << Line("           else")
         << Line("               if (d)")
         << Line("                   foo;")
         << Line("               else")
         << Line("                   a + b + ")
         << Line("                       c")
         << Line("       else")
         << Line("           foo;")
         << Line("    y: 2")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::ifStatementWithoutBraces2()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    x: {")
         << Line("        if (a)")
         << Line("            if (b)")
         << Line("                foo;")
         << Line("        if (a) b();")
         << Line("        if (a) b(); else")
         << Line("            foo;")
         << Line("        if (a)")
         << Line("            if (b)")
         << Line("                foo;")
         << Line("            else if (c)")
         << Line("                foo;")
         << Line("            else")
         << Line("                if (d)")
         << Line("                    foo;")
         << Line("                else")
         << Line("                    e")
         << Line("        else")
         << Line("            foo;")
         << Line("    }")
         << Line("    foo: bar")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::ifStatementWithBraces1()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("onClicked: {", 4)
         << Line("    if (a) {")
         << Line("        if (b) {")
         << Line("            foo;")
         << Line("        } else if (c) {")
         << Line("            foo")
         << Line("        } else {")
         << Line("            if (d) {")
         << Line("                foo")
         << Line("            } else {")
         << Line("                foo;")
         << Line("            }")
         << Line("        }")
         << Line("    } else {")
         << Line("        foo;")
         << Line("    }")
         << Line("}")
         << Line("}");
    checkIndent(data);
}

void tst_QMLCodeFormatter::ifStatementWithBraces2()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("onClicked:", 4)
         << Line("    if (a)")
         << Line("    {")
         << Line("        if (b)")
         << Line("        {")
         << Line("            foo")
         << Line("        }")
         << Line("        else if (c)")
         << Line("        {")
         << Line("            foo;")
         << Line("        }")
         << Line("        else")
         << Line("        {")
         << Line("            if (d)")
         << Line("            {")
         << Line("                foo;")
         << Line("            }")
         << Line("            else")
         << Line("            {")
         << Line("                foo")
         << Line("            }")
         << Line("        }")
         << Line("    }")
         << Line("    else")
         << Line("    {")
         << Line("        foo")
         << Line("    }")
         << Line("}");
    checkIndent(data);
}

void tst_QMLCodeFormatter::ifStatementMixed()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("onClicked:", 4)
         << Line("    if (foo)")
         << Line("        if (bar)")
         << Line("        {")
         << Line("            foo;")
         << Line("        }")
         << Line("        else")
         << Line("            if (car)")
         << Line("            {}")
         << Line("            else doo")
         << Line("    else abc")
         << Line("}");
    checkIndent(data);
}

void tst_QMLCodeFormatter::ifStatementAndComments()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("onClicked: {", 4)
         << Line("    if (foo)")
         << Line("        ; // bla")
         << Line("    else if (bar)")
         << Line("        ;")
         << Line("    if (foo)")
         << Line("        ; /*bla")
         << Line("        bla */")
         << Line("    else if (bar)")
         << Line("        // foobar")
         << Line("        ;")
         << Line("    else if (bar)")
         << Line("        /* bla")
         << Line("  bla */")
         << Line("        ;")
         << Line("}")
         << Line("}");
    checkIndent(data);
}

void tst_QMLCodeFormatter::ifStatementLongCondition()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("onClicked: {", 4)
         << Line("    if (foo &&")
         << Line("            bar")
         << Line("            || (a + b > 4")
         << Line("                && foo(bar)")
         << Line("                )")
         << Line("            ) {")
         << Line("        foo;")
         << Line("    }")
         << Line("}");
    checkIndent(data);
}

void tst_QMLCodeFormatter::strayElse()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("onClicked: {", 4)
         << Line("    while( true ) {}")
         << Line("    else", -1)
         << Line("    else {", -1)
         << Line("    }", -1)
         << Line("}");
    checkIndent(data);
}

void tst_QMLCodeFormatter::oneLineIf()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    onClicked: { if (showIt) show(); }")
         << Line("    x: 2")
         << Line("};")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::forStatement()
{
    QList<Line> data;
    data << Line("for (var i = 0; i < 20; ++i) {")
         << Line("    print(i);")
         << Line("}")
         << Line("for (var x in [a, b, c, d])")
         << Line("    x += 5")
         << Line("var z")
         << Line("for (var x in [a, b, c, d])")
         << Line("    for (;;)")
         << Line("    {")
         << Line("        for (a(); b(); c())")
         << Line("            for (a();")
         << Line("                 b(); c())")
         << Line("                for (a(); b(); c())")
         << Line("                    print(3*d)")
         << Line("    }")
         << Line("z = 2")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::whileStatement()
{
    QList<Line> data;
    data << Line("while (i < 20) {")
         << Line("    print(i);")
         << Line("}")
         << Line("while (x in [a, b, c, d])")
         << Line("    x += 5")
         << Line("var z")
         << Line("while (a + b > 0")
         << Line("       && b + c > 0)")
         << Line("    for (;;)")
         << Line("    {")
         << Line("        for (a(); b(); c())")
         << Line("            while (a())")
         << Line("                for (a(); b(); c())")
         << Line("                    print(3*d)")
         << Line("    }")
         << Line("z = 2")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::tryStatement()
{
    QList<Line> data;
    data << Line("try {")
         << Line("    print(i);")
         << Line("} catch (foo) {")
         << Line("    print(foo)")
         << Line("} finally {")
         << Line("    var z")
         << Line("    while (a + b > 0")
         << Line("           && b + c > 0)")
         << Line("        ;")
         << Line("}")
         << Line("z = 2")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::doWhile()
{
    QList<Line> data;
    data << Line("function foo() {")
         << Line("    do { if (c) foo; } while(a);")
         << Line("    do {")
         << Line("        if(a);")
         << Line("    } while(a);")
         << Line("    do")
         << Line("        foo;")
         << Line("    while(a);")
         << Line("    do foo; while(a);")
         << Line("};")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::cStyleComments()
{
    QList<Line> data;
    data << Line("/*")
         << Line("  ")
         << Line("      foo")
         << Line("      ")
         << Line("   foo")
         << Line("   ")
         << Line("*/")
         << Line("Rectangle {")
         << Line("    /*")
         << Line("      ")
         << Line("   foo")
         << Line("   ")
         << Line("    */")
         << Line("    /* bar */")
         << Line("}")
         << Line("Item {")
         << Line("    /* foo */")
         << Line("    /*")
         << Line("      ")
         << Line("   foo")
         << Line("   ")
         << Line("    */")
         << Line("    /* bar */")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::cppStyleComments()
{
    QList<Line> data;
    data << Line("// abc")
         << Line("Item {  ")
         << Line("    // ghij")
         << Line("    // def")
         << Line("    // ghij")
         << Line("    x: 4 // hik")
         << Line("    // doo")
         << Line("} // ba")
         << Line("// ba")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::ternary()
{
    QList<Line> data;
    data << Line("function foo() {")
         << Line("    var i = a ? b : c;")
         << Line("    foo += a_bigger_condition ?")
         << Line("                b")
         << Line("              : c;")
         << Line("    foo += a_bigger_condition")
         << Line("            ? b")
         << Line("            : c;")
         << Line("    var i = a ?")
         << Line("            b : c;")
         << Line("    var i = aooo ? b")
         << Line("                 : c +")
         << Line("                   2;")
         << Line("    var i = (a ? b : c) + (foo")
         << Line("                           bar);")
         << Line("}")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::switch1()
{
    QList<Line> data;
    data << Line("function foo() {")
         << Line("    switch (a) {")
         << Line("    case 1:")
         << Line("        foo;")
         << Line("        if (a);")
         << Line("    case 2:")
         << Line("    case 3: {")
         << Line("        foo")
         << Line("    }")
         << Line("    case 4:")
         << Line("    {")
         << Line("        foo;")
         << Line("    }")
         << Line("    case bar:")
         << Line("        break")
         << Line("    }")
         << Line("    case 4:")
         << Line("    {")
         << Line("        if (a) {")
         << Line("        }")
         << Line("    }")
         << Line("}")
         ;
    checkIndent(data);
}

//void tst_QMLCodeFormatter::gnuStyle()
//{
//    QList<Line> data;
//    data << Line("struct S")
//         << Line("{")
//         << Line("    void foo()")
//         << Line("    {")
//         << Line("        if (a)")
//         << Line("            {")
//         << Line("                fpp;")
//         << Line("            }")
//         << Line("        else if (b)")
//         << Line("            {")
//         << Line("                fpp;")
//         << Line("            }")
//         << Line("        else")
//         << Line("            {")
//         << Line("            }")
//         << Line("        if (b) {")
//         << Line("            fpp;")
//         << Line("        }")
//         << Line("    }")
//         << Line("};")
//         ;
//    checkIndent(data, 1);
//}

//void tst_QMLCodeFormatter::whitesmithsStyle()
//{
//    QList<Line> data;
//    data << Line("struct S")
//         << Line("    {")
//         << Line("    void foo()")
//         << Line("        {")
//         << Line("        if (a)")
//         << Line("            {")
//         << Line("            fpp;")
//         << Line("            }")
//         << Line("        if (b) {")
//         << Line("            fpp;")
//         << Line("            }")
//         << Line("        }")
//         << Line("    };")
//         ;
//    checkIndent(data, 2);
//}

void tst_QMLCodeFormatter::qmlKeywords()
{
    QList<Line> data;
    data << Line("Rectangle {")
         << Line("    on: 2")
         << Line("    property: 2")
         << Line("    signal: 2")
         << Line("    list: 2")
         << Line("    as: 2")
         << Line("    import: 2")
         << Line("    Item {")
         << Line("    }")
         << Line("    x: 2")
         << Line("};")
         ;
    checkIndent(data);
}

void tst_QMLCodeFormatter::expressionContinuation()
{
    QList<Line> data;
    data << Line("var x = 1 ? 2")
         << Line("            + 3 : 4")
         << Line("++x")
         << Line("++y--")
         << Line("x +=")
         << Line("        y++")
         << Line("var z")
         ;
    checkIndent(data);
}

QTEST_APPLESS_MAIN(tst_CodeFormatter)
#include "tst_qmlcodeformatter.moc"

