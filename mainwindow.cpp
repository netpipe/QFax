#include "mainwindow.h"
#include "ui_mainwindow.h"
//#include "linmodem/lmmain.h"
extern "C" {
int mainLM(int argc, char **argv);
int mainEfax(int argc, char **argv);
}

void test(){
char *argv1[]={"appname","-h","test"};
     int argc1 = sizeof(argv1) / sizeof(char*) - 1;

 // mainLM(argc1,argv1);

     char *argv2[]={"appname","-h","test"};
          int argc2 = sizeof(argv1) / sizeof(char*) - 1;

        // mainEfax(argc2,argv2);

}


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    test();
}

MainWindow::~MainWindow()
{
    delete ui;
}

