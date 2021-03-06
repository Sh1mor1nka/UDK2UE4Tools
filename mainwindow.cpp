/** UDK2UE4Tools: Help transfer actors, code and matinee from UDK to UE4
    Copyright (C) 2015  Roman Simenok, OlexandrI

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. **/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFile>
#include <QTextStream>
#include <qdebug.h>
#include <QClipboard>
#include <qsettings.h>
#include <QScrollBar>
#include <QStatusBar>

#define FileToSave qApp->applicationDirPath()+"/saved.txt"
#define UToReplVarType(x, y) Regs << QRegExp(QString("([\\t (,]+)((?:T?Array)?<?)%1(>?)[\\t ]+").arg(x), Qt::CaseInsensitive);\
    To << QString("\\1\\2%1\\3 ").arg(y);
/*Regs << QRegExp("var(\\(?(?:\\w+)?\\)?[\t ]*(?:transient)?)[\t ]+"+(x)+"[\t ]+", Qt::CaseInsensitive);\
    To << "var\\1 "+(y)+"";\
    Regs << QRegExp("local(\\(?(?:\\w+)?\\)?[\t ]*(?:transient)?)[\t ]+"+(x)+"[\t ]+", Qt::CaseInsensitive);\
    To << (y)+" ";*/
#define UToGetAndSetSimple(x, y) { QRegExp matcher(x, Qt::CaseInsensitive);\
int i = matcher.indexIn(str);\
if(i!=-1){\
    QStringList list = matcher.capturedTexts();\
    ResCurr+= y;\
}\
}
#define UToGetAndSetFloat(x, y, id) { QRegExp matcher(x, Qt::CaseInsensitive);\
int i = matcher.indexIn(str);\
if(i!=-1){\
    QStringList list = matcher.capturedTexts();\
    bool isOk = false;\
    float val = list[id].toFloat(&isOk);\
    if(isOk)\
        ResCurr+= y;\
}\
}

QString& QReplace(QString& Str, QRegExp What, QString To){
    Str = Str.replace(What, To);
    return Str;
}
QString& QReplace(QString& Str, QList<QRegExp>& What, QStringList& To, bool empty = true){
    int num = (std::min)(What.size(), To.size());
    for(int i =0; i<num; i++){
        Str = Str.replace(What[i], To[i]);
    }
    if(empty){
        What.clear();
        To.clear();
    }
    return Str;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    pParams(),
    TimeLockChangeEvent(false)
{
    // Ініціалізація UI
    ui->setupUi(this);
    try{// Пробуємо зчитати збережені параметри
        // Відкриваємо файл для читання
        QFile loadfile(FileToSave);
        loadfile.open(QIODevice::ReadOnly|QIODevice::Text);
        QTextStream in(&loadfile);
        // Зчитуємо все ...
        QString line = in.readAll();
        QStringList loadlist = line.split(QRegExp("\n"), QString::SkipEmptyParts);
        // ... розбираємо поелементно
        for(QString one : loadlist){
            Params Param(one);
            if(Param.valid())
                // Додаємо в список
                pParams.push_back(Param);
        }
        loadfile.close();
    }catch(...){ }

    this->refreshList();

    ConvSettCurrLocation[0] = 0.f;
    ConvSettCurrLocation[1] = 0.f;
    ConvSettCurrLocation[2] = 0.f;
    ConvSettCurrScale[0] = 1.f;
    ConvSettCurrScale[1] = 1.f;
    ConvSettCurrScale[2] = 1.f;

    statusBar()->showMessage("Program has started");
}

MainWindow::~MainWindow()
{
    try{// Пробуємо зберегти параметри
        // Відкриваємо файл для запису і очищаємо його
        QFile loadfile(FileToSave);
        loadfile.open(QIODevice::ReadWrite|QIODevice::Truncate|QIODevice::Text);
        QTextStream out(&loadfile);
        // Записуємо по елементно
        for(Params Param : pParams){
            out << Param.ToString();
        }
        // закриваємо файл
        loadfile.close();
    }catch(...){ }

    delete ui;
}

void MainWindow::on_addParams_clicked()
{
    if (ui->oldAddrText->toPlainText().isEmpty() || ui->newAddrText->toPlainText().isEmpty()) {
        return;
    }
    pParams.push_back(Params(ui->oldAddrText->toPlainText(), ui->newAddrText->toPlainText()));
    this->clearTextFields(false);
    this->refreshList();
}

void MainWindow::refreshList()
{
    ui->listWidget->clear();
    for (std::vector<Params>::iterator i = pParams.begin(); i<pParams.end(); i++){
        ui->listWidget->addItem(i->oldAddr+" | "+i->newAddr);
    }
}

void MainWindow::on_editBut_clicked()
{
    QList<QListWidgetItem*> items=ui->listWidget->selectedItems();
    if(items.size()!=1){
        return;
    }
    int id=ui->listWidget->row(items[0]);
    pParams[id].oldAddr = ui->editOldAddr->toPlainText();
    pParams[id].newAddr = ui->editNewAddr->toPlainText();

    this->clearTextFields(true);
    this->refreshList();
}

void MainWindow::on_listWidget_itemClicked(QListWidgetItem *item)
{
    int id=ui->listWidget->row(item);
    ui->editOldAddr->setPlainText(pParams[id].oldAddr);
    ui->editNewAddr->setPlainText(pParams[id].newAddr);
}

void MainWindow::on_delBut_clicked()
{
    QList<QListWidgetItem*> items=ui->listWidget->selectedItems();
    if(items.size()!=1){
        return;
    }
    int id=ui->listWidget->row(items[0]);
    pParams.erase(pParams.begin()+id);
    this->clearTextFields(true);
    this->refreshList();
}

void MainWindow::clearTextFields(bool isEditable)
{
    if (isEditable) {
        ui->editOldAddr->setPlainText("");
        ui->editNewAddr->setPlainText("");
    }else{
        ui->oldAddrText->setPlainText("");
        ui->newAddrText->setPlainText("");
    }
}

// Виправлення ініціалізації поворотів (перетворення із одиниць прийнятих в UDK до звичайних градусів)
void MainWindow::ConvertAllRotators(QString& Str, bool clearout){
    QRegExp matcher("\\(\\s*Pitch\\s*=\\s*([+-\\d.]+)\\s*,\\s*Yaw\\s*=\\s*([+-\\d.]+)\\s*,\\s*Roll\\s*=\\s*([+-\\d.]+)\\s*\\)");
    QString Cleared("");
    for(int i = matcher.indexIn(Str);i!=-1;i = matcher.indexIn(Str, i+5)){
        float Rotation[3];
        QStringList list = matcher.capturedTexts();
        QStringList::iterator it = list.begin();
        int q = 0;
        for (++it; it!=list.end(); ++it) {
            Rotation [q] = it->toFloat() * 0.00549316540360483;
            //qDebug() << Rotation[q];
            ++q;
        }
        Str = Str.replace(i, list[0].count(), QString("(Pitch=%1, Yaw=%2, Roll=%3)").arg(Rotation[0]).arg(Rotation[1]).arg(Rotation[2]));
        if(clearout) Cleared = Cleared + QString("(Pitch=%1, Yaw=%2, Roll=%3)\n").arg(Rotation[0]).arg(Rotation[1]).arg(Rotation[2]);
    }
    if(clearout) Str = Cleared;
}
/* Зміщення координат із виправленням на корегувальний скейл
 * Warning! Значення використовується в регулярному виразі!
 */
void MainWindow::CorrectAllLocation(QString& Str, float CorrectScale[3], float CorrectLoc[3], QString BeforeStr, bool clearout){
    QRegExp matcher("("+BeforeStr+")\\(\\s*X\\s*=\\s*([+-\\d.]+)\\s*,\\s*Y\\s*=\\s*([+-\\d.]+)\\s*,\\s*Z\\s*=\\s*([+-\\d.]+)\\s*\\)");
    QString Cleared("");
    for(int i = matcher.indexIn(Str); i!=-1; i = matcher.indexIn(Str, i+5)){
        float Location[3];
        QStringList list = matcher.capturedTexts();
        QStringList::iterator it = list.end() - 3;
        int q = 0;
        for (; it!=list.end(); ++it) {
            Location [q] = (it->toFloat() + CorrectLoc[q]) * CorrectScale[q];
            //qDebug() << Location[q];
            ++q;
        }
        Str = Str.replace(i, list[0].count(), QString("%1(X=%2, Y=%3, Z=%4)").arg(list[1]).arg(Location[0]).arg(Location[1]).arg(Location[2]));
        if(clearout) Cleared = Cleared + QString("(X=%1, Y=%2, Z=%3)\n").arg(Location[0]).arg(Location[1]).arg(Location[2]);
    }
    if(clearout) Str = Cleared;
}

void MainWindow::on_stayInTop_clicked()
{
    Qt::WindowFlags flags = windowFlags();
    if (ui->stayInTop->isChecked()) {
         flags |= Qt:: WindowStaysOnTopHint;
    }else{
        flags  &= ~Qt:: WindowStaysOnTopHint;
    }
    setWindowFlags(flags);
    show();
}


void MainWindow::on_UScriptSource_textChanged()
{
    QString Source(ui->UScriptSource->toPlainText());
    QList<QRegExp> Regs;
    QStringList To;

    // Change property declaration
    Regs << QRegExp("(\\s*)var\\((\\w+)\\)([\\t ]+[^<\\s]+(?:\\s*<\\s*(?:[^\\];>\r\n]+)\\s*>)?)[\\t ]+([^<;\r\n]+)<([^>\r\n]+)>;", Qt::CaseInsensitive);
    To << "\\1UPROPERTY("+ui->UScriptConvSett_DefUP->text()+", Category = \"\\2\", Meta = (\\5))\\1\\3 \\4;";
    Regs << QRegExp("(\\s*)var\\([^)]*\\)([\\t ]+[^<\\s]+(?:\\s*<\\s*(?:[^\\];>\r\n]+)\\s*>)?)[ \\t]+([^<;\r\n]+)<([^>\r\n]+)>;", Qt::CaseInsensitive);
    To << "\\1UPROPERTY("+ui->UScriptConvSett_DefUP->text()+", Meta = (\\4))\\1\\2 \\3;";
    /*Regs << QRegExp("(\\s*)var\((\\w+)\\)[ \\t]+([^<;\r\n]+)<(([^=;>\r\n]+=[^=;>\r\n]+)+)>;", Qt::CaseInsensitive);
    To << "\\1UPROPERTY(Category = \"\\2\", Meta = (\\4))\\1\\3;";*/
    Regs << QRegExp("(\\s*)var\\((\\w+)\\)[ \\t]+([^;\r\n]+);", Qt::CaseInsensitive);
    To << "\\1UPROPERTY("+ui->UScriptConvSett_DefUP->text()+", Category = \"\\2\")\\1\\3;";
    Regs << QRegExp("(\\s*)var\\(\\)[ \\t]+([^;\r\n]+);", Qt::CaseInsensitive);
    To << "\\1UPROPERTY("+ui->UScriptConvSett_DefUP->text()+")\\1\\2;";
    Regs << QRegExp("(\\s*)var[ \\t]+transient[ \\t]+([^;\r\n]+);", Qt::CaseInsensitive);
    To << "\\1\\2; // #OldNotice: Transient";
    Regs << QRegExp("(\\s*)var[ \\t]+([^;\r\n]+);", Qt::CaseInsensitive);
    To << "\\1\\2;";
    Regs << QRegExp("(\\s*)UPROPERTY\\(([^\r\n]*)\\)(\\s*)transient ([^;\r\n]+);", Qt::CaseInsensitive);
    To << "\\1UPROPERTY(Transient, \\2)\\3\\4;";
    Regs << QRegExp("Config\\s+UPROPERTY\\s*\\(", Qt::CaseInsensitive);
    To << "UPROPERTY(Config, ";
    Regs << QRegExp("GlobalConfig\\s+UPROPERTY\\s*\\(", Qt::CaseInsensitive);
    To << "UPROPERTY(GlobalConfig, ";
    Regs << QRegExp("(\\s*)UPROPERTY\\(([^\r\n]*)\\)(\\s*)Config ([^;\r\n]+);", Qt::CaseInsensitive);
    To << "\\1UPROPERTY(Config, \\2)\\3\\4;";
    Regs << QRegExp("(\\s*)UPROPERTY\\(([^\r\n]*)\\)(\\s*)GlobalConfig ([^;\r\n]+);", Qt::CaseInsensitive);
    To << "\\1UPROPERTY(GlobalConfig, \\2)\\3\\4;";
    Regs << QRegExp("(UPROPERTY\\([^\r\n]*),\\s*\\)", Qt::CaseInsensitive);
    To << "\\1)";
    Regs << QRegExp("(UPROPERTY\\()\\s*,([^\r\n]*\\))", Qt::CaseInsensitive);
    To << "\\1\\2";
    if(ui->UScriptConvSett_bOver->isChecked()){
        Regs << QRegExp("(\\s*UPROPERTY\\()[^\r\n]*(\\)\\s*[^;\r\n]+\\s+bOvv?err?ide[^;\r\n]+;)", Qt::CaseInsensitive);
        To << "\\1BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault)\\2";
    }

    // Correct property types
    Regs << QRegExp("([\\t (,]+)bool[\\t ]+([^;\r\n]+);", Qt::CaseInsensitive);
    To << "\\1uint32 \\2 : 1;";
    Regs << QRegExp("([\\t (,]+)array\\s*<\\s*([^\\];>\r\n]+)\\s*>", Qt::CaseInsensitive);
    To << "\\1TArray<\\2>";
    UToReplVarType("vector", "FVector");
    UToReplVarType("vector2d", "FVector2D");
    UToReplVarType("rotator", "FRotator");
    UToReplVarType("int", "int32");
    UToReplVarType("byte", "uint8");
    UToReplVarType("Actor", "AActor*");
    UToReplVarType("string", "FString");
    UToReplVarType("name", "FName");
    UToReplVarType("LinearColor", "FLinearColor");
    UToReplVarType("Color", "FColor");
    UToReplVarType("MAni_Info", "UCurveFloat*");
    UToReplVarType("Texture", "UTexture*");
    UToReplVarType("Texture2D", "UTexture2D*");
    UToReplVarType("TextureMovie", "UMediaTexture*");
    UToReplVarType("StaticMesh", "UStaticMesh*");
    UToReplVarType("SkeletalMesh", "USkeletalMesh*");
    UToReplVarType("ParticleSystem", "UParticleSystem*");
    UToReplVarType("SoundCue", "USoundCue*");

    QReplace(Source, Regs, To);

    // Find and change Meta in UPROPERTY
    if (Source.contains("Meta =")) {
         QRegExp matcher("Meta = \\(([^)]+)\\)", Qt::CaseInsensitive);
         QRegExp matcher2("([^=]+)\\s*=\\s*([^,]+)", Qt::CaseInsensitive);
         for(int i = matcher.indexIn(Source); i!=-1; i = matcher.indexIn(Source, i+5)){
             QStringList list = matcher.capturedTexts();
             list[1] = list[1].replace("|", ", ").replace(matcher2, "\\1 = \"\\2\"");
             Source = Source.replace(i, list[0].size(), "Meta = ("+list[1]+")");
         }
    }

    // Set auto advanced property if it has EditCondition (is override settings)
    if(ui->UScriptConvSett_AAdvanced->isChecked()){
        Regs << QRegExp("UPROPERTY\\(([^\r\n]*Meta\\s*=\\s*\\([^\r\n]*EditCondition)", Qt::CaseInsensitive);
        To << "UPROPERTY(AdvancedDisplay, \\1";
    }

    // Find and replace structdefaultproperties to empty constructor
    // Please, dont use in structure "}" or "{" (in commnet, etc),
    //  only in structdefaultproperties !
    if (Source.contains("structdefaultproperties")) {
         // 1 - struct name
         // 2 - in structrure
         // 3 - defaultproperties
         QRegExp matcher("struct\\s+([^{\\s]+)\\s*\\{([^{]+)structdefaultproperties\\{([^}]+)\\}", Qt::CaseInsensitive);
         QRegExp matcher2("([^=\\s]+)\\s*=\\s*([^\r\n]+),*", Qt::CaseInsensitive);
         for(int i = matcher.indexIn(Source); i!=-1; i = matcher.indexIn(Source, i+5)){
             QStringList list = matcher.capturedTexts();
             list[1] = list[1].trimmed();
             // Correct struct type
             UToReplVarType(list[1], "F"+list[1]);
             list[1] = list[1];
             list[3] = list[3].replace(matcher2, "\\1(\\2),").replace(",)",")").replace(";)",")")
                     .replace(QRegExp("\\(Pitch\\s*=\\s*([+-\\d.]+),\\s*Yaw\\s*=\\s*([+-\\d.]+),\\s*Roll\\s*=\\s*([+-\\d.]+)\\)"), "\\1, \\2, \\3")
                     .replace(QRegExp("\\(X\\s*=\\s*([+-\\d.]+),\\s*Y\\s*=\\s*([+-\\d.]+),\\s*Z\\s*=\\s*([+-\\d.]+)\\)"), "\\1, \\2, \\3");
             QString NewStruct = "struct "+list[1]+"{"+list[2]+"\n\tF"+list[1]+"():"+list[3]+"\n{ }";
             NewStruct = NewStruct.replace(QRegExp(",\\s*\\{ \\}"), "\n\t{ }");
             Source = Source.replace(i, list[0].size(), NewStruct);
         }
    }
    // Apply corecting struct type and change structure declaration
    Regs << QRegExp("([\t ]*)struct\\s*F?([^\\s{\r\n}]+)\\s*\\{(\\s*)", Qt::CaseInsensitive);
    To << "\\1USTRUCT("+ui->UScriptConvSett_DefUS->text()+")\r\n\\1struct F\\2\r\n\\1{\\3GENERATED_USTRUCT_BODY()\n\\3";

    // Add default uproperty for structs
    Regs << QRegExp("(struct [^{\r\n}]+\\s*\\{\\s*[^}]*)UPROPERTY\\(", Qt::CaseInsensitive);
    To << "\\1UPROPERTY("+ui->UScriptConvSett_DefSUP->text();

    // Add default ufunction for void function
    Regs << QRegExp("([\\t ]*)function\\s+(\\S+)\\s+([^{\r\n(}]+\\s*\\([^{\r\n}]+\\s*\\{)", Qt::CaseInsensitive);
    To << "\\1UFUNCTION("+ui->UScriptConvSett_DeffFun->text()+")\r\n\\1\\2 \\3";

    // Add default ufunction for function
    Regs << QRegExp("([\\t ]*)function\\s+([^{\r\n}]+\\s*\\{)", Qt::CaseInsensitive);
    To << "\\1UFUNCTION("+ui->UScriptConvSett_DeffFun->text()+")\r\n\\1void \\2";

    // Clear "local"
    Regs << QRegExp("local\\s+([^\r\n;]+);", Qt::CaseInsensitive);
    To << "\\1;";

    // Exec modoficator
    Regs << QRegExp("exec\\s+UFUNCTION\\(", Qt::CaseInsensitive);
    To << "UFUNCTION(Exec, ";

    QReplace(Source, Regs, To);

    ConvertAllRotators(Source);

    ui->UCppSource->setPlainText(Source);
}

void MainWindow::on_pasteText_textChanged()
{
        QString pTxt = ui->pasteText->toPlainText();
        // Якщо менше ніж 32 символи, то нічого не робимо (просто очищаємо)
        if(pTxt.size()<32){
            ui->newText->setPlainText("");
            return;
        }
        QList<QRegExp> Regs;
        QStringList To;

        Regs << QRegExp("\\s*Begin\\s*Map.+Begin\\s*Level", Qt::CaseInsensitive)
             << QRegExp("End\\s*Level.+Begin\\s*Surface.+End\\s*Surface.+End\\s*Map", Qt::CaseInsensitive)
             << QRegExp("\\s*End\\s*Actor", Qt::CaseInsensitive);
        To << "" << "" << "";

        // Заміняємо все, що налаштовано в параметрах
        for (Params P: pParams) {
            pTxt = pTxt.replace(P.oldAddr, P.newAddr);
        }

        // Переводимо всі повороти в градуси
        ConvertAllRotators(pTxt, false);

        // o_O
        Regs << QRegExp("\r\n") << QRegExp("\r") << QRegExp("\n\n") << QRegExp("\n");
        To << "\n" << "\n" << "\n" << "\r\n";

        QReplace(pTxt, Regs, To);
        pTxt = pTxt.trimmed();

        QString resultStr = "Begin Map\r\n   Begin Level";
        // Розбиваємо на актори
        QStringList actors = pTxt.split("Begin Actor");
        pTxt.resize(0);
        int uNumb = 0;
        float GlobalArgScale = (ConvSettCurrScale[0]+ConvSettCurrScale[1]+ConvSettCurrScale[2])/3.f;
        for (; !actors.isEmpty(); actors.pop_front()) {
            QString str = actors.first();
            str = str.trimmed();
            QString type = str.mid(6, str.indexOf(" ")-6);
            uNumb++;

            if (type == "StaticMeshActor") {
                QString ResCurr = "      Begin Actor Class=StaticMeshActor Name=StaticMeshActor_%1 Archetype=StaticMeshActor'/Script/Engine.Default__StaticMeshActor'\
\r\n         Begin Object Class=StaticMeshComponent Name=\"StaticMeshComponent0\" Archetype=StaticMeshComponent'/Script/Engine.Default__StaticMeshActor:StaticMeshComponent0'\
\r\n         End Object\
\r\n         Begin Object Name=\"StaticMeshComponent0\"";
                ResCurr = ResCurr.arg(uNumb);

                // Set StaticMesh
                UToGetAndSetSimple("\\s+StaticMesh\\s*=\\s*([^\r\n]+)", "\n            StaticMesh="+list[1].trimmed());
                // Set LightmassSettings
                UToGetAndSetSimple("\\s+LightmassSettings\\s*=\\s*([^\r\n]+)", "\n            LightmassSettings="+list[1].trimmed());
                // Set MinDrawDistance
                UToGetAndSetFloat("\\s+MinDrawDistance\\s*=\\s*([^\r\n]+)", QString("\n            MinDrawDistance=%1").arg(val*GlobalArgScale), 1);
                // Set LDMaxDrawDistance
                UToGetAndSetFloat("\\s+MaxDrawDistance\\s*=\\s*([^\r\n]+)", QString("\n            LDMaxDrawDistance=%1").arg(val*GlobalArgScale), 1);
                // Set CachedMaxDrawDistance
                UToGetAndSetFloat("\\s+CachedMaxDrawDistance\\s*=\\s*([^\r\n]+)", QString("\n            CachedMaxDrawDistance=%1").arg(val*GlobalArgScale), 1);
                // Set CastShadow
                UToGetAndSetSimple("\\s+CastShadow\\s*=\\s*([^\r\n]+)", "\n            CastShadow="+list[1].trimmed());
                // Set CastStaticShadow
                UToGetAndSetSimple("\\s+bCastStaticShadow\\s*=\\s*([^\r\n]+)", "\n            bCastStaticShadow="+list[1].trimmed());
                // Set CastDynamicShadow
                UToGetAndSetSimple("\\s+bCastDynamicShadow\\s*=\\s*([^\r\n]+)", "\n            bCastDynamicShadow="+list[1].trimmed());
                // Set SelfShadowOnly
                UToGetAndSetSimple("\\s+bSelfShadowOnly\\s*=\\s*([^\r\n]+)", "\n            bSelfShadowOnly="+list[1].trimmed());
                // Set CastHiddenShadow
                UToGetAndSetSimple("\\s+bCastHiddenShadow\\s*=\\s*([^\r\n]+)", "\n            bCastHiddenShadow="+list[1].trimmed());
                // Set CastShadowAsTwoSided
                UToGetAndSetSimple("\\s+bCastShadowAsTwoSided\\s*=\\s*([^\r\n]+)", "\n            bCastShadowAsTwoSided="+list[1].trimmed());
                // Set DetailMode
                UToGetAndSetSimple("\\s+DetailMode\\s*=\\s*([^\r\n]+)", "\n            DetailMode="+list[1].trimmed());
                // Set Location
                UToGetAndSetSimple("\\s+Location\\s*=\\s*([^\r\n]+)", "\n            RelativeLocation="+list[1].trimmed());
                // Set Rotation
                UToGetAndSetSimple("\\s+Rotation\\s*=\\s*([^\r\n]+)", "\n            RelativeRotation="+list[1].trimmed());

                { // Set Component visibility
                    QRegExp matcher("\\s+HiddenGame\\s*=\\s*True", Qt::CaseInsensitive);
                    QRegExp matcher2("\\s+HiddenEditor\\s*=\\s*True", Qt::CaseInsensitive);
                    if(matcher.indexIn(str)!=-1 || matcher2.indexIn(str)!=-1){
                        ResCurr+= "\n            bVisible=False";
                    }
                }
                { // Set RecieveDecals
                    QRegExp matcher("\\s+bAcceptsStaticDecals\\s*=\\s*False", Qt::CaseInsensitive);
                    QRegExp matcher2("\\s+bAcceptsDynamicDecals\\s*=\\s*False", Qt::CaseInsensitive);
                    if(matcher.indexIn(str)!=-1 || matcher2.indexIn(str)!=-1){
                        ResCurr+= "\n            bReceivesDecals=False";
                    }
                }

                // Set Tag
                UToGetAndSetSimple("\\s+Tag\\s*=\\s*\"([^\"\r\n]+)\"", "\n            Tag(0)=\""+list[1]+"\"");
                // Transfer Layer to Tag
                UToGetAndSetSimple("\\s+Layer\\s*=\\s*\"([^\"\r\n]+)\"", "\n            Tag(1)=\""+list[1]+"\"");

                float fDrawScale = 1.0;
                float d3Scale [3]={1.f, 1.f, 1.f};
                if (str.contains("DrawScale=")) {
                     QRegExp matcher("DrawScale=([^\r\n]+)");
                     int i = matcher.indexIn(str);
                     if (i!=-1) {
                         QStringList list = matcher.capturedTexts();
                         QStringList::iterator it = list.begin();
                         for ( ++it ;it!=list.end(); it++) {
                            fDrawScale = it->toFloat();
                         }
                     }
                     str = str.replace(QRegExp("\\s*DrawScale=[^\r\n]+[\r\n]*"), "");
                }
                if (str.contains("DrawScale3D=")) {
                     QRegExp matcher("DrawScale3D=\\(X=([+-\\d.]+),Y=([+-\\d.]+),Z=([+-\\d.]+)\\)");
                     int i = matcher.indexIn(str);
                     if (i!=-1) {
                         QStringList list = matcher.capturedTexts();
                         QStringList::iterator it = list.begin();
                         int q = 0;
                         for ( ++it ;it!=list.end(); it++) {
                             d3Scale [q++] = it->toFloat();
                         }
                     }
                     str = str.replace(QRegExp("\\s*DrawScale3D=[^\r\n]+[\r\n]*"), "");
                }
                for (int i = 0; i<3; i++) {
                    d3Scale[i] *= fDrawScale * ConvSettCurrScale[i];
                }
                QString scalePattern = "(X=%1,Y=%2,Z=%3)";
                scalePattern = scalePattern.arg(d3Scale[0]).arg(d3Scale[1]).arg(d3Scale[2]);
                ResCurr+="\n            BodyInstance=(Scale3D="+scalePattern+")\n            RelativeScale3D="+scalePattern;

                ResCurr+="\n         End Object";
                // Set bHidden
                UToGetAndSetSimple("\\s+bHidden\\s*=\\s*([^\r\n]+)", "\n            bHidden="+list[1].trimmed());

                ResCurr+=QString("\r\n         StaticMeshComponent=StaticMeshComponent0\
\r\n         RootComponent=StaticMeshComponent0\
\r\n         ActorLabel=\"StaticMeshActor_%1\"\
\r\n      End Actor").arg(uNumb);
                resultStr += "\r\n"+ResCurr;
            }else
            if(type == "PointLight" || type == "PointLightMovable"){
                QString ResCurr = "      Begin Actor Class=PointLight Name=PointLight_%1 Archetype=PointLight'/Script/Engine.Default__PointLight'\
\r\n         Begin Object Class=PointLightComponent Name=\"LightComponent0\" Archetype=PointLightComponent'/Script/Engine.Default__PointLight:LightComponent0'\
\r\n         End Object\
\r\n         Begin Object Name=\"LightComponent0\"";
                ResCurr = ResCurr.arg(uNumb);

                // Set AttenuationRadius
                UToGetAndSetFloat("\\s+Radius\\s*=\\s*([^\r\n]+)", QString("\n            AttenuationRadius=%1").arg(val*GlobalArgScale), 1);
                // Set Intensity
                UToGetAndSetFloat("\\s+Brightness\\s*=\\s*([^\r\n]+)", QString("\n            Intensity=%1").arg(val*5000.f), 1);
                // Set LightColor
                UToGetAndSetSimple("\\s+LightColor\\s*=\\s*([^\r\n]+)A", "\n            LightColor="+list[1]+"A=255)");
                // Set Location
                UToGetAndSetSimple("\\s+(?:Relative)?Location\\s*=\\s*([^\r\n]+)", "\n            RelativeLocation="+list[1]);
                // Cast StaticShadows
                UToGetAndSetSimple("\\s+CastStaticShadows\\s*=\\s*(True|False)", "\n            CastStaticShadows="+list[1]);
                // Cast DynamicShadows
                UToGetAndSetSimple("\\s+CastDynamicShadows\\s*=\\s*(True|False)", "\n            CastDynamicShadows="+list[1]);
                // Set Tag
                UToGetAndSetSimple("\\s+Tag\\s*=\\s*\"([^\"\r\n]+)\"", "\n            Tag(0)=\""+list[1]+"\"");
                // Transfer Layer to Tag
                UToGetAndSetSimple("\\s+Layer\\s*=\\s*\"([^\"\r\n]+)\"", "\n            Tag(1)=\""+list[1]+"\"");

                ResCurr+=QString("\n         End Object\
\r\n         PointLightComponent=LightComponent0\
\r\n         LightComponent=LightComponent0\
\r\n         RootComponent=LightComponent0\
\r\n         ActorLabel=\"PointLight_%1\"\
\r\n      End Actor").arg(uNumb);
                resultStr += "\r\n"+ResCurr;
            }else
            if(type == "CEEnemy_Eggs_health"){
                QString ResCurr = "      Begin Actor Class=EP_02_Egg_C Name=EP_02_Egg_C_9 Archetype=EP_02_Egg_C'/Game/EP_02/BluePrints/EP_02_Egg.Default__EP_02_Egg_C'\
\r\n         Begin Object Class=SkeletalMeshComponent Name=\"SkeletalMeshComponent0\" Archetype=SkeletalMeshComponent'/Game/EP_02/BluePrints/EP_02_Egg.Default__EP_02_Egg_C:SkeletalMeshComponent0'\
\r\n         End Object\
\r\n         Begin Object Class=SphereComponent Name=\"ActivateSphere\" Archetype=SphereComponent'/Game/EP_02/BluePrints/EP_02_Egg.EP_02_Egg_C:SphereComponent_0'\
\r\n            Begin Object Class=BodySetup Name=\"BodySetup_%1\"\
\r\n            End Object\
\r\n         End Object\
\r\n         Begin Object Class=StaticMeshComponent Name=\"Marker\" Archetype=StaticMeshComponent'/Game/EP_02/BluePrints/EP_02_Egg.EP_02_Egg_C:StaticMeshComponent_25'\
\r\n         End Object\
\r\n         Begin Object Class=StaticMeshComponent Name=\"Shell\" Archetype=StaticMeshComponent'/Game/EP_02/BluePrints/EP_02_Egg.EP_02_Egg_C:StaticMeshComponent_22'\
\r\n         End Object\
\r\n         Begin Object Class=StaticMeshComponent Name=\"Enemy\" Archetype=StaticMeshComponent'/Game/EP_02/BluePrints/EP_02_Egg.EP_02_Egg_C:StaticMeshComponent_18'\
\r\n         End Object\
\r\n         Begin Object Name=\"SkeletalMeshComponent0\"";
                ResCurr = ResCurr.arg(uNumb);

                // Set Location
                UToGetAndSetSimple("\\s+Location\\s*=\\s*([^\r\n]+)", "\n            RelativeLocation="+list[1].trimmed());
                // Set Rotation
                UToGetAndSetSimple("\\s+Rotation\\s*=\\s*([^\r\n]+)", "\n            RelativeRotation="+list[1].trimmed());

                float fDrawScale = 1.0;
                float d3Scale [3]={1.f, 1.f, 1.f};
                if (str.contains("DrawScale=")) {
                     QRegExp matcher("DrawScale=([^\r\n]+)");
                     int i = matcher.indexIn(str);
                     if (i!=-1) {
                         QStringList list = matcher.capturedTexts();
                         QStringList::iterator it = list.begin();
                         for ( ++it ;it!=list.end(); it++) {
                            fDrawScale = it->toFloat();
                         }
                     }
                     str = str.replace(QRegExp("\\s*DrawScale=[^\r\n]+[\r\n]*"), "");
                }
                if (str.contains("DrawScale3D=")) {
                     QRegExp matcher("DrawScale3D=\\(X=([+-\\d.]+),Y=([+-\\d.]+),Z=([+-\\d.]+)\\)");
                     int i = matcher.indexIn(str);
                     if (i!=-1) {
                         QStringList list = matcher.capturedTexts();
                         QStringList::iterator it = list.begin();
                         int q = 0;
                         for ( ++it ;it!=list.end(); it++) {
                             d3Scale [q++] = it->toFloat();
                         }
                     }
                     str = str.replace(QRegExp("\\s*DrawScale3D=[^\r\n]+[\r\n]*"), "");
                }
                fDrawScale /= 1.5;
                for (int i = 0; i<3; i++) {
                    d3Scale[i] *= fDrawScale * ConvSettCurrScale[i];
                }
                QString scalePattern = "(X=%1,Y=%2,Z=%3)";
                scalePattern = scalePattern.arg(d3Scale[0]).arg(d3Scale[1]).arg(d3Scale[2]);
                ResCurr+="\n            RelativeScale3D="+scalePattern;
                ResCurr+= QString("\r\n         End Object\
\r\n         Begin Object Name=\"ActivateSphere\"\
\r\n            Begin Object Name=\"BodySetup_%1\"\
\r\n               CollisionTraceFlag=CTF_UseSimpleAsComplex\
\r\n            End Object").arg(uNumb);
                // Set SphereRadius
                UToGetAndSetFloat("\\s+ActivationRadius\\s*=\\s*([^\r\n]+)", QString("\n            SphereRadius=%1").arg(val*GlobalArgScale), 1);
                ResCurr+="\n            BodyInstance="+scalePattern;
                ResCurr+= QString("            OnComponentBeginOverlap=(EP_02_Egg_C_9.BndEvt__ActivateSphere_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature)\
\r\n            OnComponentEndOverlap=(EP_02_Egg_C_9.BndEvt__ActivateSphere_K2Node_ComponentBoundEvent_5_ComponentEndOverlapSignature__DelegateSignature)\
\r\n            AttachParent=SkeletalMeshComponent0\
\r\n            bNetAddressable=True\
\r\n            CreationMethod=SimpleConstructionScript\
\r\n         End Object\
\r\n         Begin Object Name=\"Marker\"\
\r\n            BodyInstance=(Scale3D=(X=2.500000,Y=0.001250,Z=2.500000))\
\r\n            AttachParent=SkeletalMeshComponent0\
\r\n            bNetAddressable=True\
\r\n            CreationMethod=SimpleConstructionScript\
\r\n         End Object\
\r\n         Begin Object Name=\"Shell\"\
\r\n            BodyInstance=(Scale3D=(X=0.200000,Y=0.200000,Z=0.200000))\
\r\n            AttachParent=SkeletalMeshComponent0\
\r\n            bNetAddressable=True\
\r\n            CreationMethod=SimpleConstructionScript\
\r\n         End Object\
\r\n         Begin Object Name=\"Enemy\"\
\r\n            BodyInstance=(Scale3D=(X=0.125000,Y=0.125000,Z=0.125000))\
\r\n            AttachParent=SkeletalMeshComponent0\
\r\n            bNetAddressable=True\
\r\n            CreationMethod=SimpleConstructionScript\
\r\n         End Object\
\r\n         ActivateSphere=ActivateSphere\
\r\n         Marker=Marker\
\r\n         Shell=Shell\
\r\n         Enemy=Enemy").arg(uNumb);
                // Set SphereRadius
                UToGetAndSetFloat("\\s+RotateAni\\s*=\\s*\\(\\s*EndTime\\s*=\\s*([^\r\n]+)", QString("\n         RotateSpeed=(Pitch=0.000000,Yaw=%1,Roll=0.000000)").arg(360.0 / val), 1);
                // Set Health
                UToGetAndSetSimple("\\s+Health\\s*=\\s*([^\r\n]+)", "\n            Healths="+list[1].trimmed());
                // Set Tag
                UToGetAndSetSimple("\\s+Tag\\s*=\\s*\"([^\"\r\n]+)\"", "\n            Tag(0)=\""+list[1]+"\"");
                // Transfer Layer to Tag
                UToGetAndSetSimple("\\s+Layer\\s*=\\s*\"([^\"\r\n]+)\"", "\n            Tag(1)=\""+list[1]+"\"");
                // Set SphereRadius
                UToGetAndSetFloat("\\s+ActivationRadius\\s*=\\s*([^\r\n]+)", QString("\n            SphereRadius=%1").arg(val*GlobalArgScale), 1);
                ResCurr+= QString("\r\n         fxc=BP_FishEyeCamera_C'/Game/Maps/EP_02.EP_02:PersistentLevel.BP_FishEyeCamera_C_0'\
\r\n         DestroyEff=\"EggsDead\"\
\r\n         AnimationsList(0)=(Settings=(EndTime=2.000000))\
\r\n         SkeletalMeshComponent=SkeletalMeshComponent0\
\r\n         RootComponent=SkeletalMeshComponent0\
\r\n         ActorLabel=\"EP_02_Egg%1\"\
\r\n      End Actor").arg(uNumb);
                resultStr += "\r\n"+ResCurr;
            }
        }
        actors.clear();
        MainWindow::CorrectAllLocation(resultStr, ConvSettCurrScale, ConvSettCurrLocation);
        ui->newText->setPlainText(resultStr+"\r\n   End Level\r\nBegin Surface\r\nEnd Surface\r\nEnd Map");
        resultStr.resize(0);
        if(ui->ConvAutoCopy->isChecked()) QApplication::clipboard()->setText(ui->newText->toPlainText());
}

void MainWindow::on_toolButton_clicked()
{
    ui->UScriptSource->setPlainText(QApplication::clipboard()->text());
}

void MainWindow::on_toolButton_2_clicked()
{
    QApplication::clipboard()->setText(ui->UCppSource->toPlainText());
}

void MainWindow::syncUSourceScroll1(int nPos){
    QScrollBar* bar = ui->UCppSource->verticalScrollBar();
    nPos = (float)bar->maximum() / (float)ui->UScriptSource->verticalScrollBar()->maximum() * nPos;
    bar->setSliderPosition(nPos);
}
void MainWindow::syncUSourceScroll2(int nPos){
    QScrollBar* bar = ui->UScriptSource->verticalScrollBar();
    nPos = (float)bar->maximum() / (float)ui->UCppSource->verticalScrollBar()->maximum() * nPos;
    bar->setSliderPosition(nPos);
}

void MainWindow::on_toolButton_3_toggled(bool checked)
{
    if(checked){
        connect(ui->UScriptSource->verticalScrollBar(),&QScrollBar::sliderMoved,this,&MainWindow::syncUSourceScroll1);
    }else{
        disconnect(ui->UScriptSource->verticalScrollBar(),&QScrollBar::sliderMoved,this,&MainWindow::syncUSourceScroll1);
    }
}

void MainWindow::on_toolButton_4_toggled(bool checked)
{
    if(checked){
        connect(ui->UCppSource->verticalScrollBar(),&QScrollBar::sliderMoved,this,&MainWindow::syncUSourceScroll2);
    }else{
        disconnect(ui->UCppSource->verticalScrollBar(),&QScrollBar::sliderMoved,this,&MainWindow::syncUSourceScroll2);
    }
}

void MainWindow::refreshConvScaleEditLine(){
    TimeLockChangeEvent = true;
    ui->ConvSettScaleX->setText(QString("%1").arg(ConvSettCurrScale[0]));
    ui->ConvSettScaleY->setText(QString("%1").arg(ConvSettCurrScale[1]));
    ui->ConvSettScaleZ->setText(QString("%1").arg(ConvSettCurrScale[2]));
    TimeLockChangeEvent = false;
}
void MainWindow::refreshConvLocEditLine(){
    TimeLockChangeEvent = true;
    ui->ConvSettLocatX->setText(QString("%1").arg(ConvSettCurrLocation[0]));
    ui->ConvSettLocatY->setText(QString("%1").arg(ConvSettCurrLocation[1]));
    ui->ConvSettLocatZ->setText(QString("%1").arg(ConvSettCurrLocation[2]));
    TimeLockChangeEvent = false;
}

bool MainWindow::setValWithRelative(QLineEdit* From, unsigned int Id, float* Array, bool lock)
{
    if(Id>2) return false;
    QString input(From->text());
    input = input.replace(QRegExp("[^-+\\d\\.,]+"), "").replace(",", ".");
    bool isOk = false;
    float val = input.toFloat(&isOk);

    if(!isOk || input.size()==0){
        From->setStyleSheet("border: 1px solid red;");
        return false;
    }else From->setStyleSheet("border: 1px solid green;");

    if(val==Array[Id]) return false;

    if(lock && Array[Id]!=0.f && val!=0.f){
        float ch = val / Array[Id];
        switch(Id){
        case 1:
            Array[0]*= ch;
            Array[2]*= ch;
            break;
        case 2:
            Array[1]*= ch;
            Array[0]*= ch;
            break;
        default:
            Array[1]*= ch;
            Array[2]*= ch;
            break;
        }
    }

    Array[Id] = val;

    return true;
}

void MainWindow::on_oldRotationText_textChanged(){
    if(ui->HelperAutoClear->isChecked()){
        QString StrR(ui->oldRotationText->toPlainText());
        QString StrL(ui->oldRotationText->toPlainText());
        ConvertAllRotators(StrR, true);
        CorrectAllLocation(StrL, ConvSettCurrScale, ConvSettCurrLocation, ui->HelperClearPrefix->text(), true);
        ui->newRotationText->setPlainText(StrR+StrL);
    }else{
        QString Str(ui->oldRotationText->toPlainText());
        ConvertAllRotators(Str, false);
        CorrectAllLocation(Str, ConvSettCurrScale, ConvSettCurrLocation, "", false);
        ui->newRotationText->setPlainText(Str);
    }
    if(ui->HelperAutoCopy->isChecked()) QApplication::clipboard()->setText(ui->newRotationText->toPlainText());
}

void MainWindow::on_HelperAutoClear_toggled(bool checked)
{
    on_oldRotationText_textChanged();
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
    switch(index){
    case 2:
        on_oldRotationText_textChanged();
        break;
    case 1:
        on_pasteText_textChanged();
        break;
    }
}

void MainWindow::on_ConvSettScaleX_editingFinished()
{
    if(!TimeLockChangeEvent && setValWithRelative(ui->ConvSettScaleX, 0, ConvSettCurrScale, ui->ConvSettScaleLock->isChecked()))
        refreshConvScaleEditLine();
}

void MainWindow::on_ConvSettScaleY_editingFinished()
{
    if(!TimeLockChangeEvent && setValWithRelative(ui->ConvSettScaleY, 1, ConvSettCurrScale, ui->ConvSettScaleLock->isChecked()))
        refreshConvScaleEditLine();
}

void MainWindow::on_ConvSettScaleZ_editingFinished()
{
    if(!TimeLockChangeEvent && setValWithRelative(ui->ConvSettScaleZ, 2, ConvSettCurrScale, ui->ConvSettScaleLock->isChecked()))
        refreshConvScaleEditLine();
}

void MainWindow::on_ConvSettLocatX_editingFinished()
{
    if(!TimeLockChangeEvent && setValWithRelative(ui->ConvSettLocatX, 0, ConvSettCurrLocation, ui->ConvSettLocatLock->isChecked()))
        refreshConvLocEditLine();
}


void MainWindow::on_ConvSettLocatY_editingFinished()
{
    if(!TimeLockChangeEvent && setValWithRelative(ui->ConvSettLocatY, 1, ConvSettCurrLocation, ui->ConvSettLocatLock->isChecked()))
        refreshConvLocEditLine();
}

void MainWindow::on_ConvSettLocatZ_editingFinished()
{
    if(!TimeLockChangeEvent && setValWithRelative(ui->ConvSettLocatZ, 2, ConvSettCurrLocation, ui->ConvSettLocatLock->isChecked()))
        refreshConvLocEditLine();
}

void MainWindow::on_toolButton_5_clicked()
{
    ConvSettCurrScale[0] = 1.f / ConvSettCurrScale[0];
    ConvSettCurrScale[1] = 1.f / ConvSettCurrScale[1];
    ConvSettCurrScale[2] = 1.f / ConvSettCurrScale[2];
    refreshConvScaleEditLine();
}
