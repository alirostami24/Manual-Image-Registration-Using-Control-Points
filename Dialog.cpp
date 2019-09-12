#include "Dialog.h"
#include "ui_Dialog.h"

CDialog::CDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CDialog)
{
    ui->setupUi(this);
}

CDialog::~CDialog()
{
    delete ui;
}
