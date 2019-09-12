#include "Mainui.h"
#include "ui_mainui.h"

MainUI::MainUI(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainUI)
{
    ui->setupUi(this);

    ui->lblReferenceImage->installEventFilter(this);
    ui->lblMovingImage->installEventFilter(this);

    disableReferenceImageMouseMove = false;
    disableMovingImageMouseMove = false;

    referencePointIsSelected = false;
    movingPointIsSelected = false;

    isReferenceAvailable = false;
    isMovingAvailable = false;

    haveSelectedPair = false;

    selectedPairIndex = -1;

    isRegisterationDone = false;

    zoomFactor = 5;

    isActiveMovingImageCLAHE = true;
    isActiveReferenceImageCLAHE = false;

    pairPointCounter = 0;
    ui->lblPairPointCounter->setText(QString("Selected Pair-Point Counts: " + QString::number(pairPointCounter)));

    controlPointDialog = new CDialog();
    connect(controlPointDialog, &CDialog::accepted, this, &MainUI::sltReceiveOkForControlPointRemoval);

    QImage QIm(ui->lblReferenceImage->width(), ui->lblReferenceImage->height(),QImage::Format_RGB888);
    QIm.fill(Qt::blue);
    ui->lblReferenceImage->setPixmap(QPixmap::fromImage(QIm));
    ui->lblMovingImage->setPixmap(QPixmap::fromImage(QIm));
    ui->lblZoomedReference->setPixmap(QPixmap::fromImage(QIm));
    ui->lblZoomedMoving->setPixmap(QPixmap::fromImage(QIm));

    QImage QImRegistered(ui->lblRegisteredImage->width(), ui->lblRegisteredImage->height(),QImage::Format_RGB888);
    QImRegistered.fill(Qt::blue);
    ui->lblRegisteredImage->setPixmap(QPixmap::fromImage(QImRegistered));

    ui->toolButton->addAction(ui->actionAffine);
    ui->toolButton->addAction(ui->actionHomography);

    ui->lblVersion->setText("Version: 1.3.2.1032");

    method = ETransformationType(Affine);
    ui->lblMethod->setText("Affine");
    ui->lblNumPointRequired->setText("The Required Number of Points: 3");
}

MainUI::~MainUI()
{
    delete controlPointDialog;
    delete ui;
}

void MainUI::on_btnRegister_clicked()
{
    if((method == ETransformationType::Affine && pairPointCounter > 2) || (method == ETransformationType::Homography && pairPointCounter > 3))
    {
        QString fileName = QFileDialog::getSaveFileName(this,
                                                        tr("Select Path for Transition Matrix"), ""
                                                        , tr("*.txt"));

        if(fileName.isEmpty())
        {
            qDebug() << "The specified file name is empty!";
            fileName = "outputTransforamtionMatrix.txt";
        }

        QFileInfo InputMap(fileName);
        QString fullName = fileName;
        if(InputMap.suffix() == "")
            fullName = QString(fullName + ".txt");

        cv::Mat affineWarpMat(2, 3, CV_32FC1);
        cv::Mat homographyWarpMat(3, 3, CV_32FC1);

        std::vector<cv::Mat> channels;
        cv::Mat imgPair;

        std::ofstream outputTransforamtionMatrix;
        outputTransforamtionMatrix.open(fileName.toLatin1().data());

        switch (method)
        {
        case ETransformationType::Affine:

            affineWarpMat = cv::getAffineTransform(movingPointsForAffine, referencePointsForAffine);

            cv::warpAffine(convertedMovingImage, warpDst, affineWarpMat, warpDst.size());

            if (outputTransforamtionMatrix.is_open())
            {
                outputTransforamtionMatrix << affineWarpMat.at<float>(0, 0) << ",     " << affineWarpMat.at<float>(0, 1) << ",     " << affineWarpMat.at<float>(0, 2) << "\n";
                outputTransforamtionMatrix << affineWarpMat.at<float>(1, 0) << ",     " << affineWarpMat.at<float>(1, 1) << ",     " << affineWarpMat.at<float>(1, 2) << "\n";
            }

            break;
        case ETransformationType::Homography:

            // Find homography
            homographyWarpMat = findHomography(movingPointsForHomography, referencePointsForHomography, cv::RANSAC );//LMEDS,RANSAC,RHO

            // Use homography to warp image
            cv::warpPerspective(convertedMovingImage, warpDst, homographyWarpMat, warpDst.size());

            if (outputTransforamtionMatrix.is_open())
            {
                outputTransforamtionMatrix << homographyWarpMat.at<float>(0, 0) << ",     " << homographyWarpMat.at<float>(0, 1) << ",     " << homographyWarpMat.at<float>(0, 2) << "\n";
                outputTransforamtionMatrix << homographyWarpMat.at<float>(1, 0) << ",     " << homographyWarpMat.at<float>(1, 1) << ",     " << homographyWarpMat.at<float>(1, 2) << "\n";
                outputTransforamtionMatrix << homographyWarpMat.at<float>(2, 0) << ",     " << homographyWarpMat.at<float>(2, 1) << ",     " << homographyWarpMat.at<float>(2, 2) << "\n";
            }

            break;
        }

        outputTransforamtionMatrix.close();

        cv::Mat paddedImage;
        cv::Mat matOriginal_Gray;
        cv::Mat warpDst_Gray;
        cv::cvtColor(convertedReferenceImage, matOriginal_Gray, CV_RGB2GRAY);
        cv::cvtColor(warpDst, warpDst_Gray, CV_RGB2GRAY);

        if(matOriginal_Gray.cols >= warpDst.cols)
        {
            paddedImage.create(matOriginal_Gray.rows, matOriginal_Gray.cols, CV_8UC1);
            paddedImage.setTo(0);

            cv::Mat ROI(paddedImage, cv::Rect(0, 0, warpDst.cols, warpDst.rows));
            warpDst_Gray.copyTo(ROI);

            channels.push_back(matOriginal_Gray);
            channels.push_back(paddedImage);
            channels.push_back(matOriginal_Gray);


            merge(channels, imgPair);
        }
        else
        {
            paddedImage.create(warpDst.rows, warpDst.cols, CV_8UC1);
            paddedImage.setTo(0);
            cv::Mat ROI(paddedImage, cv::Rect(0, 0, matOriginal_Gray.cols, matOriginal_Gray.rows));
            matOriginal_Gray.copyTo(ROI);

            channels.push_back(warpDst_Gray);
            channels.push_back(paddedImage);
            channels.push_back(warpDst_Gray);

            merge(channels, imgPair);
        }

        if (!imShowPair.empty())
            imShowPair.release();

        imShowPair = imgPair.clone();

        QImage qimgOriginal_Pre((uchar*)imgPair.data, imgPair.cols, imgPair.rows, imgPair.step, QImage::Format_RGB888);
        ui->lblRegisteredImage->setPixmap(QPixmap::fromImage(qimgOriginal_Pre));

        isRegisterationDone = true;
    }
    else
    {
        QMessageBox msgBox;
        if(method == ETransformationType::Affine)
            msgBox.setText("Registration Failed! At least three pair points should be selected!");
        else if(method == ETransformationType::Homography)
            msgBox.setText("Registration Failed! At least four pair points should be selected!");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();
    }
}

void MainUI::on_btnLoadReference_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open Image"), "E:/bistco/RGB-T DataSet/RGB-T234/baketballwaliking/reference/"
                                                    , tr("Map Files (*.jpg *.tif *.bmp *.png)"));

    QFileInfo fi(fileName);
    referenceImagePath = fi.path();

    if(fileName.isEmpty())
    {
        qDebug() << "The specified file name is empty!";
        return;
    }

    cv::Mat matOriginal = cv::imread(fileName.toStdString());
    cv::cvtColor(matOriginal, convertedReferenceImage, CV_BGR2RGB);

    if (isActiveReferenceImageCLAHE)
    {
        cv::cvtColor(matOriginal, convertedReferenceImage_CLAHE, CV_RGB2GRAY);
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
        clahe->setClipLimit(4);
        clahe->apply(convertedReferenceImage_CLAHE, convertedReferenceImage_CLAHE);

        if(!enlargedReference.empty())
            enlargedReference.release();
        cv::resize(convertedReferenceImage_CLAHE, enlargedReference, cv::Size(zoomFactor*matOriginal.cols, zoomFactor*matOriginal.rows), 0,0,cv::INTER_LINEAR);

        QImage qimgOriginal_Pre((uchar*)convertedReferenceImage_CLAHE.data, convertedReferenceImage_CLAHE.cols, convertedReferenceImage_CLAHE.rows, convertedReferenceImage_CLAHE.step, QImage::Format_Grayscale8);
        ui->lblReferenceImage->setPixmap(QPixmap::fromImage(qimgOriginal_Pre));
    }
    else
    {
        if(!enlargedReference.empty())
            enlargedReference.release();
        cv::resize(convertedReferenceImage, enlargedReference, cv::Size(zoomFactor*matOriginal.cols, zoomFactor*matOriginal.rows), 0,0,cv::INTER_LINEAR);

        QImage qimgOriginal_Pre((uchar*)convertedReferenceImage.data, convertedReferenceImage.cols, convertedReferenceImage.rows, convertedReferenceImage.step, QImage::Format_RGB888);
        ui->lblReferenceImage->setPixmap(QPixmap::fromImage(qimgOriginal_Pre));
    }

    updateReferenceImage();

    disableReferenceImageMouseMove = false;
    isReferenceAvailable = true;

    ui->dsbReferenceX->setMaximum(matOriginal.cols);
    ui->dsbReferenceX->setSingleStep(1.0/zoomFactor);
    ui->dsbReferenceY->setMaximum(matOriginal.rows);
    ui->dsbReferenceY->setSingleStep(1.0/zoomFactor);
}

void MainUI::on_btnLoadMoving_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open Image"), "E:/bistco/RGB-T DataSet/RGB-T234/baketballwaliking/infrared"
                                                    , tr("Map Files (*.jpg *.tif *.bmp)"));

    if(fileName.isEmpty())
    {
        qDebug() << "The specified file name is empty!";
        return;
    }

    QFileInfo fi(fileName);
    movingImageSuffix = fi.suffix();
    movingImagePath = fi.path();

    cv::Mat matOriginal = cv::imread(fileName.toStdString());
    cv::cvtColor(matOriginal, convertedMovingImage, CV_BGR2RGB);

    if (isActiveMovingImageCLAHE)
    {
        cv::cvtColor(matOriginal, convertedMovingImage_CLAHE, CV_RGB2GRAY);
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
        clahe->setClipLimit(4);
        clahe->apply(convertedMovingImage_CLAHE, convertedMovingImage_CLAHE);

        if(!enlargedMoving.empty())
            enlargedMoving.release();
        cv::resize(convertedMovingImage_CLAHE, enlargedMoving, cv::Size(zoomFactor*matOriginal.cols, zoomFactor*matOriginal.rows), 0,0,cv::INTER_LINEAR);

        QImage qimgOriginal_Pre((uchar*)convertedMovingImage_CLAHE.data, convertedMovingImage_CLAHE.cols, convertedMovingImage_CLAHE.rows, convertedMovingImage_CLAHE.step, QImage::Format_Grayscale8);
        ui->lblMovingImage->setPixmap(QPixmap::fromImage(qimgOriginal_Pre));
    }
    else
    {
        if(!enlargedMoving.empty())
            enlargedMoving.release();
        cv::resize(convertedMovingImage, enlargedMoving, cv::Size(zoomFactor*matOriginal.cols, zoomFactor*matOriginal.rows), 0,0,cv::INTER_LINEAR);

        QImage qimgOriginal_Pre((uchar*)convertedMovingImage.data, convertedMovingImage.cols, convertedMovingImage.rows, convertedMovingImage.step, QImage::Format_RGB888);
        ui->lblMovingImage->setPixmap(QPixmap::fromImage(qimgOriginal_Pre));
    }

    updateMovingImage();

    disableMovingImageMouseMove = false;
    isMovingAvailable = true;

    ui->dsbMovingX->setMaximum(matOriginal.cols);
    ui->dsbMovingX->setSingleStep(1.0/zoomFactor);
    ui->dsbMovingY->setMaximum(matOriginal.rows);
    ui->dsbMovingY->setSingleStep(1.0/zoomFactor);
}

bool MainUI::eventFilter(QObject *dist, QEvent *event)
{
    QMouseEvent *me = static_cast<QMouseEvent *>(event);

    if (me->button() == Qt::MiddleButton && haveSelectedPair)
    {
        haveSelectedPair = false;

        updateReferenceImage();
        updateMovingImage();
    }

    if (dist == ui->lblReferenceImage && isReferenceAvailable)
    {
        referenceRatioX = (float) convertedReferenceImage.cols/ui->lblReferenceImage->width();
        referenceRatioY = (float) convertedReferenceImage.rows/ui->lblReferenceImage->height();

        if (event->type() == QEvent::MouseMove && !disableReferenceImageMouseMove)
        {
            QPoint coordinates = me->pos();

            pReference.setX(referenceRatioX*coordinates.x());
            pReference.setY(referenceRatioY*coordinates.y());

            plotReferenceZoomedImage();
        }
        else if (me->button() == Qt::RightButton
                 && event->type() == QEvent::MouseButtonRelease
                 && event->type() != QEvent::MouseMove)
        {
            selectedPairIndex = -1;
            float distance = 10000;
            float currentDistance;
            for (int i = 0; i < pairPointCounter; i++)
            {
                switch (method)
                {
                case ETransformationType::Affine:
                    currentDistance = qSqrt(qPow(referencePointsForAffine[i].x - pReference.x(),2.0)
                                            + qPow(referencePointsForAffine[i].y - pReference.y(), 2.0));

                    break;
                case ETransformationType::Homography:
                    currentDistance = qSqrt(qPow(referencePointsForHomography[i].x - pReference.x(),2.0)
                                            + qPow(referencePointsForHomography[i].y - pReference.y(), 2.0));
                    break;
                }

                if (currentDistance < distance)
                {
                    haveSelectedPair = true;
                    distance = currentDistance;
                    selectedPairIndex = i;
                }
            }

            updateReferenceImage();
            updateMovingImage();
        }
        else if (event->type() == QEvent::MouseButtonRelease
                 && me->button() != Qt::RightButton
                 && me->button() != Qt::MiddleButton
                 && !disableReferenceImageMouseMove)
        {
            disableReferenceImageMouseMove = true;

            cv::Mat fullWindow;
            cv::resize(convertedReferenceImage, fullWindow, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);
            cv::circle(fullWindow, cv::Point((float)pReference.x()/referenceRatioX, (float)pReference.y()/referenceRatioY), 3, cv::Scalar(255,0,0), 4,-1);

            if(pairPointCounter>0)
            {
                for (int i=0; i<pairPointCounter; i++)
                {
                    cv::circle(fullWindow, cv::Point((float)referencePointsForAffine[i].x/referenceRatioX, (float)referencePointsForAffine[i].y/referenceRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                }
            }

            if (isActiveReferenceImageCLAHE)
            {
                cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
                clahe->setClipLimit(4);
                cv::cvtColor(convertedReferenceImage, convertedReferenceImage_CLAHE, CV_RGB2GRAY);
                clahe->apply(convertedReferenceImage_CLAHE, convertedReferenceImage_CLAHE);

                cv::Mat CLAHERGB;
                cv::cvtColor(convertedReferenceImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
                cv::resize(CLAHERGB, fullWindow, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);
            }

            QImage qimg((uchar*)fullWindow.data, fullWindow.cols, fullWindow.rows, fullWindow.step, QImage::Format_RGB888);
            ui->lblReferenceImage->setPixmap(QPixmap::fromImage(qimg));

            ui->dsbReferenceX->setValue(pReference.x());
            ui->dsbReferenceY->setValue(pReference.y());

            referencePointIsSelected = true;
        }
    }

    if (dist == ui->lblMovingImage && isMovingAvailable)
    {
        movingRatioX = (float) convertedMovingImage.cols/ui->lblMovingImage->width();
        movingRatioY = (float) convertedMovingImage.rows/ui->lblMovingImage->height();

        if (event->type() == QEvent::MouseMove && !disableMovingImageMouseMove)
        {
            QPoint coordinates = me->pos();

            pMoving.setX(movingRatioX*coordinates.x());
            pMoving.setY(movingRatioY*coordinates.y());

            plotMovingZoomedImage();
        }
        else if (me->button() == Qt::RightButton
                 && event->type() == QEvent::MouseButtonRelease
                 && event->type() != QEvent::MouseMove)
        {
            selectedPairIndex = -1;
            float distance = 10000;
            float currentDistance;
            for (int i = 0; i < pairPointCounter; i++)
            {
                switch (method)
                {
                case ETransformationType::Affine:
                    currentDistance = qSqrt(qPow(movingPointsForAffine[i].x - pMoving.x(),2.0)
                                            + qPow(movingPointsForAffine[i].y - pMoving.y(), 2.0));

                    break;
                case ETransformationType::Homography:
                    currentDistance = qSqrt(qPow(movingPointsForHomography[i].x - pMoving.x(),2.0)
                                            + qPow(movingPointsForHomography[i].y - pMoving.y(), 2.0));
                    break;
                }

                if (currentDistance < distance)
                {
                    haveSelectedPair = true;
                    distance = currentDistance;
                    selectedPairIndex = i;
                }
            }

            updateReferenceImage();
            updateMovingImage();
        }
        else if (event->type() == QEvent::MouseButtonRelease
                 && me->button() != Qt::RightButton
                 && me->button() != Qt::MiddleButton
                 && !disableMovingImageMouseMove)
        {
            disableMovingImageMouseMove = true;

            cv::Mat fullWindow;
            cv::resize(convertedMovingImage, fullWindow, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);
            cv::circle(fullWindow, cv::Point((float)pMoving.x()/movingRatioX, (float)pMoving.y()/movingRatioY), 3, cv::Scalar(255,0,0), 4,-1);

            if(pairPointCounter > 0)
            {
                for (int i=0; i<pairPointCounter; i++)
                {
                    cv::circle(fullWindow, cv::Point((float)movingPointsForAffine[i].x/movingRatioX, (float)movingPointsForAffine[i].y/movingRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                }
            }

            if (isActiveMovingImageCLAHE)
            {
                cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
                clahe->setClipLimit(4);
                cv::cvtColor(convertedMovingImage, convertedMovingImage_CLAHE, CV_RGB2GRAY);
                clahe->apply(convertedMovingImage_CLAHE, convertedMovingImage_CLAHE);

                cv::Mat CLAHERGB;
                cv::cvtColor(convertedMovingImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
                cv::resize(CLAHERGB, fullWindow, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);
            }

            QImage qimg((uchar*)fullWindow.data, fullWindow.cols, fullWindow.rows, fullWindow.step, QImage::Format_RGB888);
            ui->lblMovingImage->setPixmap(QPixmap::fromImage(qimg));

            ui->dsbMovingX->setValue(pMoving.x());
            ui->dsbMovingY->setValue(pMoving.y());

            movingPointIsSelected = true;
        }

    }
    return QObject::eventFilter(dist, event);
}

void MainUI::on_btnSavePoint_clicked()
{
    if (!disableReferenceImageMouseMove || !disableMovingImageMouseMove)
    {
        QMessageBox msgBox;
        msgBox.setText("A single point cannot be saved, you should select a pair!");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();
    }
    else
    {
        cv::Mat fullWindowReference;
        if (isActiveReferenceImageCLAHE)
        {
            qDebug() << isActiveReferenceImageCLAHE;
            cv::Mat CLAHERGB;
            cv::cvtColor(convertedReferenceImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
            cv::resize(CLAHERGB, fullWindowReference, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);
        }
        else
            cv::resize(convertedReferenceImage, fullWindowReference, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);

        cv::Mat fullWindowMoving;
        if (isActiveMovingImageCLAHE)
        {
            cv::Mat CLAHERGB;
            cv::cvtColor(convertedMovingImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
            cv::resize(CLAHERGB, fullWindowMoving, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);
        }
        else
            cv::resize(convertedMovingImage, fullWindowMoving, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);

        switch (method)
        {
        case ETransformationType::Affine:
            if (pairPointCounter < 3 && referencePointIsSelected && movingPointIsSelected)
            {
                movingPointsForAffine[pairPointCounter] = cv::Point2f(pMoving.x(), pMoving.y());
                referencePointsForAffine[pairPointCounter] = cv::Point2f(pReference.x(), pReference.y());

                disableReferenceImageMouseMove = false;
                disableMovingImageMouseMove = false;

                referencePointIsSelected = false;
                movingPointIsSelected = false;

                pairPointCounter++;
                ui->lblPairPointCounter->setText(QString("Selected Pair-Point Counts: " + QString::number(pairPointCounter)));


                if(pairPointCounter>0)
                {
                    for (int i=0; i<pairPointCounter; i++)
                    {
                        cv::circle(fullWindowReference, cv::Point((float)referencePointsForAffine[i].x/referenceRatioX, (float)referencePointsForAffine[i].y/referenceRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                        cv::circle(fullWindowMoving, cv::Point((float)movingPointsForAffine[i].x/movingRatioX, (float)movingPointsForAffine[i].y/movingRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                    }
                }

                QImage qimgOriginal_PreReference((uchar*)fullWindowReference.data, fullWindowReference.cols, fullWindowReference.rows, fullWindowReference.step, QImage::Format_RGB888);
                ui->lblReferenceImage->setPixmap(QPixmap::fromImage(qimgOriginal_PreReference));

                QImage qimgOriginal_PreMoving((uchar*)fullWindowMoving.data, fullWindowMoving.cols, fullWindowMoving.rows, fullWindowMoving.step, QImage::Format_RGB888);
                ui->lblMovingImage->setPixmap(QPixmap::fromImage(qimgOriginal_PreMoving));
            }
            else
            {
                QMessageBox msgBox;
                msgBox.setText("You are allowed to choose at most three points for Affine transform!\nShould you require to renew any point, a previously saved point must be eliminated!");
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.exec();
            }
            break;

        case ETransformationType::Homography:
            referencePointsForHomography.push_back(cv::Point2f(pReference.x(), pReference.y()));
            movingPointsForHomography.push_back(cv::Point2f(pMoving.x(), pMoving.y()));

            disableReferenceImageMouseMove = false;
            disableMovingImageMouseMove = false;

            referencePointIsSelected = false;
            movingPointIsSelected = false;

            pairPointCounter++;
            ui->lblPairPointCounter->setText(QString("Selected Pair-Point Counts: " + QString::number(pairPointCounter)));

            if(pairPointCounter>0)
            {
                for (int i=0; i<pairPointCounter; i++)
                {
                    cv::circle(fullWindowReference, cv::Point((float)referencePointsForHomography[i].x/referenceRatioX, (float)referencePointsForHomography[i].y/referenceRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                    cv::circle(fullWindowMoving, cv::Point((float)movingPointsForHomography[i].x/movingRatioX, (float)movingPointsForHomography[i].y/movingRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                }
            }

            QImage qimgOriginal_PreReference((uchar*)fullWindowReference.data, fullWindowReference.cols, fullWindowReference.rows, fullWindowReference.step, QImage::Format_RGB888);
            ui->lblReferenceImage->setPixmap(QPixmap::fromImage(qimgOriginal_PreReference));

            QImage qimgOriginal_PreMoving((uchar*)fullWindowMoving.data, fullWindowMoving.cols, fullWindowMoving.rows, fullWindowMoving.step, QImage::Format_RGB888);
            ui->lblMovingImage->setPixmap(QPixmap::fromImage(qimgOriginal_PreMoving));

            break;
        }
    }
}

void MainUI::on_btnRemoveCurrentPoints_clicked()
{
    disableReferenceImageMouseMove = false;
    disableMovingImageMouseMove = false;

    referencePointIsSelected = false;
    movingPointIsSelected = false;

    updateReferenceImage();
    updateMovingImage();
}

void MainUI::updateReferenceImage()
{
    if (isReferenceAvailable)
    {
        cv::Mat fullWindowReference;

        if(isActiveReferenceImageCLAHE)
        {
            cv::Mat CLAHERGB;
            cv::cvtColor(convertedReferenceImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
            cv::resize(CLAHERGB, fullWindowReference, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);
        }
        else
            cv::resize(convertedReferenceImage, fullWindowReference, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);

        if(pairPointCounter>0)
        {
            switch (method)
            {
            case ETransformationType::Affine:
                for (int i=0; i< qMin(3, pairPointCounter) ; i++)
                {
                    if (selectedPairIndex == i && haveSelectedPair)
                        cv::circle(fullWindowReference, cv::Point((float)referencePointsForAffine[i].x/referenceRatioX, (float)referencePointsForAffine[i].y/referenceRatioY), 3, cv::Scalar(0,0,255), 4,-1);
                    else
                        cv::circle(fullWindowReference, cv::Point((float)referencePointsForAffine[i].x/referenceRatioX, (float)referencePointsForAffine[i].y/referenceRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                }

                break;

            case ETransformationType::Homography:

                for (int i=0; i<pairPointCounter; i++)
                {
                    if (selectedPairIndex == i && haveSelectedPair)
                        cv::circle(fullWindowReference, cv::Point((float)referencePointsForHomography[i].x/referenceRatioX, (float)referencePointsForHomography[i].y/referenceRatioX), 3, cv::Scalar(0,0,255), 4,-1);
                    else
                        cv::circle(fullWindowReference, cv::Point((float)referencePointsForHomography[i].x/referenceRatioX, (float)referencePointsForHomography[i].y/referenceRatioX), 3, cv::Scalar(0,255,0), 4,-1);
                }

                break;
            }
        }

        QImage qimgOriginal_PreReference((uchar*)fullWindowReference.data, fullWindowReference.cols, fullWindowReference.rows, fullWindowReference.step, QImage::Format_RGB888);
        ui->lblReferenceImage->setPixmap(QPixmap::fromImage(qimgOriginal_PreReference));
    }
}

void MainUI::updateMovingImage()
{
    if (isMovingAvailable)
    {
        cv::Mat fullWindowMoving;

        if(isActiveMovingImageCLAHE)
        {
            cv::Mat CLAHERGB;
            cv::cvtColor(convertedMovingImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
            cv::resize(CLAHERGB, fullWindowMoving, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);
        }
        else
            cv::resize(convertedMovingImage, fullWindowMoving, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);

        if(pairPointCounter>0)
        {
            switch (method)
            {
            case ETransformationType::Affine:
                for (int i=0; i< qMin(3, pairPointCounter); i++)
                {
                    if (selectedPairIndex == i && haveSelectedPair)
                        cv::circle(fullWindowMoving, cv::Point((float)movingPointsForAffine[i].x/movingRatioX, (float)movingPointsForAffine[i].y/movingRatioY), 3, cv::Scalar(0,0,255), 4,-1);
                    else
                        cv::circle(fullWindowMoving, cv::Point((float)movingPointsForAffine[i].x/movingRatioX, (float)movingPointsForAffine[i].y/movingRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                }

                break;

            case ETransformationType::Homography:

                for (int i=0; i<pairPointCounter; i++)
                {
                    if (selectedPairIndex == i && haveSelectedPair)
                        cv::circle(fullWindowMoving, cv::Point((float)movingPointsForHomography[i].x/movingRatioX, (float)movingPointsForHomography[i].y/movingRatioY), 3, cv::Scalar(0,0,255), 4,-1);
                    else
                        cv::circle(fullWindowMoving, cv::Point((float)movingPointsForHomography[i].x/movingRatioX, (float)movingPointsForHomography[i].y/movingRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                }
                break;
            }
        }

        QImage qimgOriginal_PreMoving((uchar*)fullWindowMoving.data, fullWindowMoving.cols, fullWindowMoving.rows, fullWindowMoving.step, QImage::Format_RGB888);
        ui->lblMovingImage->setPixmap(QPixmap::fromImage(qimgOriginal_PreMoving));
    }
}


void MainUI::plotReferenceZoomedImage()
{
    cv::Rect rectOnZoomed = cv::Rect(qRound(pReference.x()*zoomFactor - ui->lblZoomedReference->width()/2.0)
                                     , qRound(pReference.y()*zoomFactor - ui->lblZoomedReference->height()/2.0)
                                     , qMin(ui->lblZoomedReference->width(), enlargedReference.cols - qRound(pReference.x()*zoomFactor - ui->lblZoomedReference->width()/2.0))
                                     , qMin(ui->lblZoomedReference->height(), enlargedReference.rows - qRound(pReference.y()*zoomFactor - ui->lblZoomedReference->height()/2.0)));

    int offsetX = 0;
    int offsetY = 0;

    if (rectOnZoomed.x < 0)
    {
        offsetX = rectOnZoomed.x;
        rectOnZoomed.width += offsetX;
        rectOnZoomed.x = 0;
    }
    if (rectOnZoomed.y < 0)
    {
        offsetY = rectOnZoomed.y;
        rectOnZoomed.height += offsetY;
        rectOnZoomed.y = 0;
    }
    if (rectOnZoomed.width < 0)
        rectOnZoomed.width = 0;
    if (rectOnZoomed.height < 0)
        rectOnZoomed.height = 0;

    cv::Mat ROIOnZoomed(enlargedReference, rectOnZoomed);
    cv::Mat ROIClone;
    ROIClone = ROIOnZoomed.clone();
    if(ROIClone.channels() == 1)
        cv::cvtColor(ROIClone, ROIClone, CV_GRAY2RGB);

    cv::Mat resizedIm(ui->lblZoomedReference->height(), ui->lblZoomedReference->width(), convertedReferenceImage.type());
    resizedIm.setTo(0);

    int min_x;
    int min_y;
    if (pReference.x() < ui->lblZoomedReference->width()/zoomFactor)
    {
        min_x = resizedIm.cols - ROIClone.cols;
    }
    else
        min_x = 0;


    if (pReference.y() < ui->lblZoomedReference->height()/referenceRatioY*zoomFactor)
        min_y = resizedIm.rows - ROIClone.rows;
    else
        min_y = 0;

    if (ROIClone.cols > 0 && ROIClone.rows >0)
    {
        cv::Mat A_roi(resizedIm, cv::Rect(min_x, min_y, ROIClone.cols, ROIClone.rows));
        ROIClone.copyTo(A_roi);
    }

    cv::Mat zoomedWindow;
    cv::resize(resizedIm, zoomedWindow, cv::Size(ui->lblZoomedReference->width(), ui->lblZoomedReference->height()), 0,0,cv::INTER_LINEAR);
    cv::line(zoomedWindow, cv::Point(ui->lblZoomedReference->width()/2.0, 0), cv::Point(ui->lblZoomedReference->width()/2.0, ui->lblZoomedReference->height()), cv::Scalar(0,255,0));
    cv::line(zoomedWindow, cv::Point(0, ui->lblZoomedReference->height()/2.0), cv::Point(ui->lblZoomedReference->width(), ui->lblZoomedReference->height()/2.0), cv::Scalar(0,255,0));

    QImage qimg((uchar*)zoomedWindow.data, zoomedWindow.cols, zoomedWindow.rows, zoomedWindow.step, QImage::Format_RGB888);
    ui->lblZoomedReference->setPixmap(QPixmap::fromImage(qimg));
}

void MainUI::plotMovingZoomedImage()
{
    cv::Rect rectOnZoomed = cv::Rect(qRound(pMoving.x()*zoomFactor - ui->lblZoomedMoving->width()/2.0)
                                     , qRound(pMoving.y()*zoomFactor - ui->lblZoomedMoving->height()/2.0)
                                     , qMin(ui->lblZoomedMoving->width(), enlargedMoving.cols - qRound(pMoving.x()*zoomFactor - ui->lblZoomedMoving->width()/2.0))
                                     , qMin(ui->lblZoomedMoving->height(), enlargedMoving.rows - qRound(pMoving.y()*zoomFactor - ui->lblZoomedMoving->height()/2.0)));

    int offsetX = 0;
    int offsetY = 0;

    if (rectOnZoomed.x < 0)
    {
        offsetX = rectOnZoomed.x;
        rectOnZoomed.width += offsetX;
        rectOnZoomed.x = 0;
    }
    if (rectOnZoomed.y < 0)
    {
        offsetY = rectOnZoomed.y;
        rectOnZoomed.height += offsetY;
        rectOnZoomed.y = 0;
    }
    if (rectOnZoomed.width < 0)
        rectOnZoomed.width = 0;
    if (rectOnZoomed.height < 0)
        rectOnZoomed.height = 0;

    cv::Mat ROIOnZoomed(enlargedMoving, rectOnZoomed);
    cv::Mat ROIClone;
    ROIClone = ROIOnZoomed.clone();
    if(ROIClone.channels() == 1)
        cv::cvtColor(ROIClone, ROIClone, CV_GRAY2RGB);

    cv::Mat resizedIm(ui->lblZoomedMoving->height(), ui->lblZoomedMoving->width(), convertedMovingImage.type());
    resizedIm.setTo(0);

    int min_x;
    int min_y;
    if (pMoving.x() < ui->lblZoomedMoving->width()/zoomFactor)
    {
        min_x = resizedIm.cols - ROIClone.cols;
    }
    else
        min_x = 0;


    if (pMoving.y() < ui->lblZoomedMoving->height()/movingRatioY*zoomFactor)
        min_y = resizedIm.rows - ROIClone.rows;
    else
        min_y = 0;

    if (ROIClone.cols > 0 && ROIClone.rows >0)
    {
        cv::Mat A_roi(resizedIm, cv::Rect(min_x, min_y, ROIClone.cols, ROIClone.rows));
        ROIClone.copyTo(A_roi);
    }

    cv::Mat zoomedWindow;
    cv::resize(resizedIm, zoomedWindow, cv::Size(ui->lblZoomedMoving->width(), ui->lblZoomedMoving->height()), 0,0,cv::INTER_LINEAR);
    cv::line(zoomedWindow, cv::Point(ui->lblZoomedMoving->width()/2.0, 0), cv::Point(ui->lblZoomedMoving->width()/2.0, ui->lblZoomedMoving->height()), cv::Scalar(0,255,0));
    cv::line(zoomedWindow, cv::Point(0, ui->lblZoomedMoving->height()/2.0), cv::Point(ui->lblZoomedMoving->width(), ui->lblZoomedMoving->height()/2.0), cv::Scalar(0,255,0));

    QImage qimg((uchar*)zoomedWindow.data, zoomedWindow.cols, zoomedWindow.rows, zoomedWindow.step, QImage::Format_RGB888);
    ui->lblZoomedMoving->setPixmap(QPixmap::fromImage(qimg));
}

void MainUI::on_btnSaveRegistered_clicked()
{
    if (isRegisterationDone)
    {
        QString fileName = QFileDialog::getSaveFileName(this,
                                                        tr("Select Path for Registered Image"), ""
                                                        , tr(""));

        if(fileName.isEmpty())
        {
            fileName = QString("RegisteredImage." + movingImageSuffix);
        }


        QFileInfo InputMap(fileName);
        QString fullName = fileName;
        if(InputMap.suffix() == "")
            fullName = QString(fullName + "." + movingImageSuffix);


        cv::cvtColor(warpDst, warpDst,CV_BGR2RGB);
        cv::imwrite(fullName.toLatin1().data(), warpDst);
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText("No Registered Images Available!");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();
    }
}

void MainUI::on_btnClear_clicked()
{
    disableReferenceImageMouseMove = false;
    disableMovingImageMouseMove = false;

    isReferenceAvailable = false;
    isMovingAvailable = false;

    isRegisterationDone = false;

    pairPointCounter = 0;
    ui->lblPairPointCounter->setText(QString("Selected Pair-Point Counts: " + QString::number(pairPointCounter)));

    movingPointsForHomography.clear();
    referencePointsForHomography.clear();

    for (int i = 0; i<3; i++)
    {
        referencePointsForAffine[i] = cv::Point2f(0.0,0.0);
        movingPointsForAffine[i] = cv::Point2f(0.0,0.0);
    }

    QImage QIm(ui->lblReferenceImage->width(), ui->lblReferenceImage->height(),QImage::Format_RGB888);
    QIm.fill(Qt::blue);
    ui->lblReferenceImage->setPixmap(QPixmap::fromImage(QIm));
    ui->lblMovingImage->setPixmap(QPixmap::fromImage(QIm));
    ui->lblZoomedReference->setPixmap(QPixmap::fromImage(QIm));
    ui->lblZoomedMoving->setPixmap(QPixmap::fromImage(QIm));

    QImage QImRegistered(ui->lblRegisteredImage->width(), ui->lblRegisteredImage->height(),QImage::Format_RGB888);
    QImRegistered.fill(Qt::blue);
    ui->lblRegisteredImage->setPixmap(QPixmap::fromImage(QImRegistered));
}

void MainUI::on_dsbReferenceX_valueChanged(double arg1)
{
    pReference.setX(arg1);

    drawReferenceZoomed();
}

void MainUI::on_dsbReferenceY_valueChanged(double arg1)
{
    pReference.setY(arg1);

    drawReferenceZoomed();
}

void MainUI::drawReferenceZoomed()
{
    cv::Rect rectOnZoomed = cv::Rect(qRound(pReference.x()*zoomFactor - ui->lblZoomedReference->width()/2.0)
                                     , qRound(pReference.y()*zoomFactor - ui->lblZoomedReference->height()/2.0)
                                     , qMin(ui->lblZoomedReference->width(), enlargedReference.cols - qRound(pReference.x()*zoomFactor - ui->lblZoomedReference->width()/2.0))
                                     , qMin(ui->lblZoomedReference->height(), enlargedReference.rows - qRound(pReference.y()*zoomFactor - ui->lblZoomedReference->height()/2.0)));

    int offsetX = 0;
    int offsetY = 0;

    if (rectOnZoomed.x < 0)
    {
        offsetX = rectOnZoomed.x;
        rectOnZoomed.width += offsetX;
        rectOnZoomed.x = 0;
    }
    if (rectOnZoomed.y < 0)
    {
        offsetY = rectOnZoomed.y;
        rectOnZoomed.height += offsetY;
        rectOnZoomed.y = 0;
    }
    if (rectOnZoomed.width < 0)
        rectOnZoomed.width = 0;
    if (rectOnZoomed.height < 0)
        rectOnZoomed.height = 0;

    cv::Mat ROIOnZoomed(enlargedReference, rectOnZoomed);
    cv::Mat ROIClone;
    ROIClone = ROIOnZoomed.clone();
    if(ROIClone.channels() == 1)
        cv::cvtColor(ROIClone, ROIClone, CV_GRAY2RGB);

    cv::Mat resizedIm(ui->lblZoomedReference->height(), ui->lblZoomedReference->width(), convertedReferenceImage.type());
    resizedIm.setTo(0);

    int min_x;
    int min_y;
    if (pReference.x() < ui->lblZoomedReference->width()/zoomFactor)
    {
        min_x = resizedIm.cols - ROIClone.cols;
    }
    else
        min_x = 0;


    if (pReference.y() < ui->lblZoomedReference->height()/referenceRatioY*zoomFactor)
        min_y = resizedIm.rows - ROIClone.rows;
    else
        min_y = 0;
    if (ROIClone.cols > 0 && ROIClone.rows >0)
    {
        cv::Mat A_roi(resizedIm, cv::Rect(min_x, min_y, ROIClone.cols, ROIClone.rows));
        ROIClone.copyTo(A_roi);
    }

    cv::Mat zoomedWindow;
    cv::resize(resizedIm, zoomedWindow, cv::Size(ui->lblZoomedReference->width(), ui->lblZoomedReference->height()), 0,0,cv::INTER_LINEAR);
    cv::line(zoomedWindow, cv::Point(ui->lblZoomedReference->width()/2.0, 0), cv::Point(ui->lblZoomedReference->width()/2.0, ui->lblZoomedReference->height()), cv::Scalar(0,255,0));
    cv::line(zoomedWindow, cv::Point(0, ui->lblZoomedReference->height()/2.0), cv::Point(ui->lblZoomedReference->width(), ui->lblZoomedReference->height()/2.0), cv::Scalar(0,255,0));

    QImage qimg((uchar*)zoomedWindow.data, zoomedWindow.cols, zoomedWindow.rows, zoomedWindow.step, QImage::Format_RGB888);
    ui->lblZoomedReference->setPixmap(QPixmap::fromImage(qimg));

    cv::Mat fullWindow;
    if (isActiveReferenceImageCLAHE)
    {
        cv::Mat CLAHERGB;
        cv::cvtColor(convertedReferenceImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
        cv::resize(CLAHERGB, fullWindow, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);
    }
    else
        cv::resize(convertedReferenceImage, fullWindow, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);

    cv::circle(fullWindow, cv::Point((float)pReference.x()/referenceRatioX, (float)pReference.y()/referenceRatioY), 3, cv::Scalar(255,0,0), 4,-1);
    QImage qimgOriginal_Pre((uchar*)fullWindow.data, fullWindow.cols, fullWindow.rows, fullWindow.step, QImage::Format_RGB888);
    ui->lblReferenceImage->setPixmap(QPixmap::fromImage(qimgOriginal_Pre));
}

void MainUI::on_dsbMovingX_valueChanged(double arg1)
{
    pMoving.setX(arg1);

    drawMovingZoomed();
}

void MainUI::on_dsbMovingY_valueChanged(double arg1)
{
    pMoving.setY(arg1);

    drawMovingZoomed();
}

void MainUI::drawMovingZoomed()
{
    cv::Rect rectOnZoomed = cv::Rect(qRound(pMoving.x()*zoomFactor - ui->lblZoomedMoving->width()/2.0)
                                     , qRound(pMoving.y()*zoomFactor - ui->lblZoomedMoving->height()/2.0)
                                     , qMin(ui->lblZoomedMoving->width(), enlargedMoving.cols - qRound(pMoving.x()*zoomFactor - ui->lblZoomedMoving->width()/2.0))
                                     , qMin(ui->lblZoomedMoving->height(), enlargedMoving.rows - qRound(pMoving.y()*zoomFactor - ui->lblZoomedMoving->height()/2.0)));

    int offsetX = 0;
    int offsetY = 0;

    if (rectOnZoomed.x < 0)
    {
        offsetX = rectOnZoomed.x;
        rectOnZoomed.width += offsetX;
        rectOnZoomed.x = 0;
    }
    if (rectOnZoomed.y < 0)
    {
        offsetY = rectOnZoomed.y;
        rectOnZoomed.height += offsetY;
        rectOnZoomed.y = 0;
    }
    if (rectOnZoomed.width < 0)
        rectOnZoomed.width = 0;
    if (rectOnZoomed.height < 0)
        rectOnZoomed.height = 0;

    cv::Mat ROIOnZoomed(enlargedMoving, rectOnZoomed);
    cv::Mat ROIClone;
    ROIClone = ROIOnZoomed.clone();
    if(ROIClone.channels() == 1)
        cv::cvtColor(ROIClone, ROIClone, CV_GRAY2RGB);

    cv::Mat resizedIm(ui->lblZoomedMoving->height(), ui->lblZoomedMoving->width(), convertedMovingImage.type());
    resizedIm.setTo(0);

    int min_x;
    int min_y;
    if (pMoving.x() < ui->lblZoomedMoving->width()/zoomFactor)
    {
        min_x = resizedIm.cols - ROIClone.cols;
    }
    else
        min_x = 0;


    if (pMoving.y() < ui->lblZoomedMoving->height()/referenceRatioY*zoomFactor)
        min_y = resizedIm.rows - ROIClone.rows;
    else
        min_y = 0;
    if (ROIClone.cols > 0 && ROIClone.rows >0)
    {
        cv::Mat A_roi(resizedIm, cv::Rect(min_x, min_y, ROIClone.cols, ROIClone.rows));
        ROIClone.copyTo(A_roi);
    }

    cv::Mat zoomedWindow;
    cv::resize(resizedIm, zoomedWindow, cv::Size(ui->lblZoomedMoving->width(), ui->lblZoomedMoving->height()), 0,0,cv::INTER_LINEAR);
    cv::line(zoomedWindow, cv::Point(ui->lblZoomedMoving->width()/2.0, 0), cv::Point(ui->lblZoomedMoving->width()/2.0, ui->lblZoomedMoving->height()), cv::Scalar(0,255,0));
    cv::line(zoomedWindow, cv::Point(0, ui->lblZoomedMoving->height()/2.0), cv::Point(ui->lblZoomedMoving->width(), ui->lblZoomedMoving->height()/2.0), cv::Scalar(0,255,0));

    QImage qimg((uchar*)zoomedWindow.data, zoomedWindow.cols, zoomedWindow.rows, zoomedWindow.step, QImage::Format_RGB888);
    ui->lblZoomedMoving->setPixmap(QPixmap::fromImage(qimg));

    cv::Mat fullWindow;
    if (isActiveMovingImageCLAHE)
    {
        cv::Mat CLAHERGB;
        cv::cvtColor(convertedMovingImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
        cv::resize(CLAHERGB, fullWindow, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);
    }
    else
        cv::resize(convertedMovingImage, fullWindow, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);

    cv::circle(fullWindow, cv::Point((float)pMoving.x()/movingRatioX, (float)pMoving.y()/movingRatioY), 3, cv::Scalar(255,0,0), 4,-1);
    QImage qimgOriginal_Pre((uchar*)fullWindow.data, fullWindow.cols, fullWindow.rows, fullWindow.step, QImage::Format_RGB888);
    ui->lblMovingImage->setPixmap(QPixmap::fromImage(qimgOriginal_Pre));
}

void MainUI::on_btnSaveImshowPair_clicked()
{
    if (isRegisterationDone)
    {
        QString fileName = QFileDialog::getSaveFileName(this,
                                                        tr("Select Path for ImshowPair Image"), ""
                                                        , tr(""));

        if(fileName.isEmpty())
        {
            fileName = QString("ImShowPair." + movingImageSuffix);
        }


        QFileInfo InputMap(fileName);
        QString fullName = fileName;
        if(InputMap.suffix() == "")
            fullName = QString(fullName + "." + movingImageSuffix);


        cv::cvtColor(imShowPair, imShowPair,CV_BGR2RGB);
        cv::imwrite(fullName.toLatin1().data(), imShowPair);
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText("No Registered Images Available!");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();
    }
}

void MainUI::on_actionAffine_triggered()
{
    controlPointDialog->show();
}

void MainUI::on_actionHomography_triggered()
{
    method = ETransformationType(Homography);
    ui->lblMethod->setText("Homography");
    ui->lblNumPointRequired->setText("The Required Number of Points: >4,\n(7 to 10 works fine!)");

    referencePointsForHomography.clear();
    movingPointsForHomography.clear();

    for (int i=0; i < qMin(3, pairPointCounter); i++)
    {
        referencePointsForHomography.push_back(cv::Point2f(referencePointsForAffine[i].x, referencePointsForAffine[i].y));
        movingPointsForHomography.push_back(cv::Point2f(movingPointsForAffine[i].x, movingPointsForAffine[i].y));
    }
    
    pairPointCounter = 3;
}

void MainUI::sltReceiveOkForControlPointRemoval()
{
    method = ETransformationType(Affine);
    ui->lblMethod->setText("Affine");
    ui->lblNumPointRequired->setText("The Required Number of Points: 3");

    if (isReferenceAvailable && isMovingAvailable)
    {
        pairPointCounter = qMin(3, pairPointCounter);

        for (int i=0; i < qMin(3, pairPointCounter) ; i++)
        {
            referencePointsForAffine[i] = cv::Point2f(referencePointsForHomography[i].x, referencePointsForHomography[i].y);
            movingPointsForAffine[i] = cv::Point2f(movingPointsForHomography[i].x, movingPointsForHomography[i].y);
        }

        referencePointsForHomography.clear();
        movingPointsForHomography.clear();

        updateReferenceImage();
        updateMovingImage();
    }
}

void MainUI::on_chbMovingContrastEnhancement_toggled(bool checked)
{
    isActiveMovingImageCLAHE = checked;

    if (isMovingAvailable)
    {
        if(checked)
        {
            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
            clahe->setClipLimit(4);
            cv::cvtColor(convertedMovingImage, convertedMovingImage_CLAHE, CV_RGB2GRAY);
            clahe->apply(convertedMovingImage_CLAHE, convertedMovingImage_CLAHE);

            if(!enlargedMoving.empty())
                enlargedMoving.release();
            cv::resize(convertedMovingImage_CLAHE, enlargedMoving,
                       cv::Size(zoomFactor*convertedMovingImage_CLAHE.cols, zoomFactor*convertedMovingImage_CLAHE.rows), 0,0,cv::INTER_LINEAR);

            cv::Mat CLAHERGB;
            cv::Mat fullWindowMoving;
            cv::cvtColor(convertedMovingImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
            cv::resize(CLAHERGB, fullWindowMoving, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);

            if(disableMovingImageMouseMove)
                cv::circle(fullWindowMoving, cv::Point((float)pMoving.x()/movingRatioX, (float)pMoving.y()/movingRatioY), 3, cv::Scalar(255,0,0), 4,-1);
            else
            {
                if(pairPointCounter>0)
                {
                    switch (method)
                    {
                    case ETransformationType::Affine:
                        for (int i=0; i< qMin(3, pairPointCounter) ; i++)
                        {
                            cv::circle(fullWindowMoving, cv::Point((float)movingPointsForAffine[i].x/movingRatioX, (float)movingPointsForAffine[i].y/movingRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                        }

                        break;

                    case ETransformationType::Homography:

                        for (int i=0; i<pairPointCounter; i++)
                        {
                            cv::circle(fullWindowMoving, cv::Point((float)movingPointsForHomography[i].x/movingRatioX, (float)movingPointsForHomography[i].y/movingRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                        }
                        break;
                    }
                }
            }

            QImage qimgOriginal_Pre((uchar*)fullWindowMoving.data, fullWindowMoving.cols, fullWindowMoving.rows, fullWindowMoving.step, QImage::Format_RGB888);
            ui->lblMovingImage->setPixmap(QPixmap::fromImage(qimgOriginal_Pre));
        }
        else
        {
            if(!enlargedMoving.empty())
                enlargedMoving.release();
            cv::resize(convertedMovingImage, enlargedMoving, cv::Size(zoomFactor*convertedMovingImage.cols, zoomFactor*convertedMovingImage.rows), 0,0,cv::INTER_LINEAR);

            cv::Mat fullWindowMoving;
            cv::resize(convertedMovingImage, fullWindowMoving,
                       cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);

            if(disableMovingImageMouseMove)
            {
                cv::circle(fullWindowMoving, cv::Point((float)pMoving.x()/movingRatioX, (float)pMoving.y()/movingRatioY), 3, cv::Scalar(255,0,0), 4,-1);
                drawMovingZoomed();
            }
            else
            {
                if(pairPointCounter>0)
                {
                    switch (method)
                    {
                    case ETransformationType::Affine:
                        for (int i=0; i< qMin(3, pairPointCounter) ; i++)
                        {
                            cv::circle(fullWindowMoving, cv::Point((float)movingPointsForAffine[i].x/movingRatioX, (float)movingPointsForAffine[i].y/movingRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                        }

                        break;

                    case ETransformationType::Homography:

                        for (int i=0; i<pairPointCounter; i++)
                        {
                            cv::circle(fullWindowMoving, cv::Point((float)movingPointsForHomography[i].x/movingRatioX, (float)movingPointsForHomography[i].y/movingRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                        }
                        break;
                    }
                }
            }

            QImage qimgOriginal_Pre((uchar*)fullWindowMoving.data, fullWindowMoving.cols, fullWindowMoving.rows, fullWindowMoving.step, QImage::Format_RGB888);
            ui->lblMovingImage->setPixmap(QPixmap::fromImage(qimgOriginal_Pre));
        }
    }
}

void MainUI::on_chbReferenceContrastEnhancement_toggled(bool checked)
{
    isActiveReferenceImageCLAHE = checked;

    if (isReferenceAvailable)
    {
        if(checked)
        {
            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
            clahe->setClipLimit(4);
            cv::cvtColor(convertedReferenceImage, convertedReferenceImage_CLAHE, CV_RGB2GRAY);
            clahe->apply(convertedReferenceImage_CLAHE, convertedReferenceImage_CLAHE);

            if(!enlargedReference.empty())
                enlargedReference.release();
            cv::resize(convertedReferenceImage_CLAHE, enlargedReference,
                       cv::Size(zoomFactor*convertedReferenceImage_CLAHE.cols, zoomFactor*convertedReferenceImage_CLAHE.rows), 0,0,cv::INTER_LINEAR);

            cv::Mat CLAHERGB;
            cv::Mat fullWindowReference;
            cv::cvtColor(convertedReferenceImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
            cv::resize(CLAHERGB, fullWindowReference, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);

            if(disableReferenceImageMouseMove)
                cv::circle(fullWindowReference, cv::Point((float)pReference.x()/referenceRatioX, (float)pReference.y()/referenceRatioY), 3, cv::Scalar(255,0,0), 4,-1);
            else
            {
                if(pairPointCounter>0)
                {
                    switch (method)
                    {
                    case ETransformationType::Affine:
                        for (int i=0; i< qMin(3, pairPointCounter) ; i++)
                        {
                            cv::circle(fullWindowReference, cv::Point((float)referencePointsForAffine[i].x/referenceRatioX, (float)referencePointsForAffine[i].y/referenceRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                        }

                        break;

                    case ETransformationType::Homography:

                        for (int i=0; i<pairPointCounter; i++)
                        {
                            cv::circle(fullWindowReference, cv::Point((float)referencePointsForHomography[i].x/referenceRatioX, (float)referencePointsForHomography[i].y/referenceRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                        }
                        break;
                    }
                }
            }

            QImage qImg((uchar*)fullWindowReference.data, fullWindowReference.cols, fullWindowReference.rows, fullWindowReference.step, QImage::Format_RGB888);
            ui->lblReferenceImage->setPixmap(QPixmap::fromImage(qImg));
        }
        else
        {
            if(!enlargedReference.empty())
                enlargedReference.release();
            cv::resize(convertedReferenceImage, enlargedReference, cv::Size(zoomFactor*convertedReferenceImage.cols, zoomFactor*convertedReferenceImage.rows), 0,0,cv::INTER_LINEAR);

            cv::Mat fullWindowReference;
            cv::resize(convertedReferenceImage, fullWindowReference,
                       cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);

            if(disableReferenceImageMouseMove)
            {
                cv::circle(fullWindowReference, cv::Point((float)pReference.x()/referenceRatioX, (float)pReference.y()/referenceRatioY), 3, cv::Scalar(255,0,0), 4,-1);
                drawReferenceZoomed();
            }
            else
            {
                if(pairPointCounter>0)
                {
                    switch (method)
                    {
                    case ETransformationType::Affine:
                        for (int i=0; i< qMin(3, pairPointCounter) ; i++)
                        {
                            cv::circle(fullWindowReference, cv::Point((float)referencePointsForAffine[i].x/referenceRatioX, (float)referencePointsForAffine[i].y/referenceRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                        }

                        break;

                    case ETransformationType::Homography:

                        for (int i=0; i<pairPointCounter; i++)
                        {
                            cv::circle(fullWindowReference, cv::Point((float)referencePointsForHomography[i].x/referenceRatioX, (float)referencePointsForHomography[i].y/referenceRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                        }
                        break;
                    }
                }
            }

            QImage qImg((uchar*)fullWindowReference.data, fullWindowReference.cols, fullWindowReference.rows, fullWindowReference.step, QImage::Format_RGB888);
            ui->lblReferenceImage->setPixmap(QPixmap::fromImage(qImg));
        }
    }
}

void MainUI::on_btnSaveControlPoints_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Select Path for Control Points"), ""
                                                    , tr("*.txt"));

    if(fileName.isEmpty())
    {
        qDebug() << "The specified file name is empty!";
        fileName = "controlPoints.txt";
    }

    QFileInfo InputMap(fileName);
    QString fullName = fileName;
    if(InputMap.suffix() == "")
        fullName = QString(fullName + ".txt");

    std::ofstream controlPoints;
    controlPoints.open(fileName.toLatin1().data());

    if (controlPoints.is_open())
    {
        switch (method)
        {
        case ETransformationType::Affine:
            controlPoints << "Affine\n";
            for (int i=0; i< qMin(3, pairPointCounter); i++)
            {
                controlPoints << (float)referencePointsForAffine[i].x << "," << (float)referencePointsForAffine[i].y
                              << "," << (float)movingPointsForAffine[i].x << "," <<(float)movingPointsForAffine[i].y << "\n";
            }

            break;
        case ETransformationType::Homography:
            controlPoints << "Homography\n";
            for (int i=0; i<pairPointCounter; i++)
            {
                controlPoints << (float)referencePointsForHomography[i].x << "," << (float)referencePointsForHomography[i].y
                              << "," << (float)movingPointsForHomography[i].x << "," << (float)movingPointsForHomography[i].y << "\n";
            }

            break;
        }
        controlPoints.close();
    }
}

void MainUI::on_btnLoadControlPoints_clicked()
{
    if (isReferenceAvailable && isMovingAvailable)
    {
        QString fileName = QFileDialog::getOpenFileName(this,
                                                        tr("Select the file containing Control Points"), ""
                                                        , tr("*.txt"));

        if(fileName.isEmpty())
        {
            qDebug() << "The specified file name is empty!";
            return;
        }

        QFile file(fileName);
        QString line;
        pairPointCounter = 0;
        if (file.open(QIODevice::ReadOnly))
        {
            QTextStream stream(&file);
            while (!stream.atEnd())
            {
                line = stream.readLine();
                if (line== "Affine")
                {
                    method = ETransformationType(Affine);

                    ui->lblMethod->setText("Affine");
                    ui->lblNumPointRequired->setText("The Required Number of Points: 3");

                    for (int i = 0; i<3; i++)
                    {
                        referencePointsForAffine[i] = cv::Point2f(0.0,0.0);
                        movingPointsForAffine[i] = cv::Point2f(0.0,0.0);
                    }
                }
                else if(line == "Homography")
                {
                    method = ETransformationType(Homography);

                    ui->lblMethod->setText("Homography");
                    ui->lblNumPointRequired->setText("The Required Number of Points: >4,\n(7 to 10 works fine!)");

                    referencePointsForHomography.clear();
                    movingPointsForHomography.clear();
                }
                else if (line != "")
                {
                    QStringList list = line.split(",");
                    if(method == ETransformationType(Affine))
                    {
                        referencePointsForAffine[pairPointCounter] = cv::Point2f(list[0].toFloat(), list[1].toFloat());
                        movingPointsForAffine[pairPointCounter] = cv::Point2f(list[2].toFloat(), list[3].toFloat());
                    }
                    else if(ETransformationType(Homography))
                    {
                        referencePointsForHomography.push_back(cv::Point2f(list[0].toFloat(), list[1].toFloat()));
                        movingPointsForHomography.push_back(cv::Point2f(list[2].toFloat(), list[3].toFloat()));
                    }
                    pairPointCounter++;
                    ui->lblPairPointCounter->setText(QString("Selected Pair-Point Counts: " + QString::number(pairPointCounter)));
                }
            }

            cv::Mat fullWindowReference;
            if (isActiveReferenceImageCLAHE)
            {
                cv::Mat CLAHERGB;
                cv::cvtColor(convertedReferenceImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
                cv::resize(CLAHERGB, fullWindowReference, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);
            }
            else
                cv::resize(convertedReferenceImage, fullWindowReference, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);

            cv::Mat fullWindowMoving;
            if (isActiveMovingImageCLAHE)
            {
                cv::Mat CLAHERGB;
                cv::cvtColor(convertedMovingImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
                cv::resize(CLAHERGB, fullWindowMoving, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);
            }
            else
                cv::resize(convertedMovingImage, fullWindowMoving, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);

            if (isReferenceAvailable && isMovingAvailable)
            {
                disableReferenceImageMouseMove = false;
                disableMovingImageMouseMove = false;

                referencePointIsSelected = false;
                movingPointIsSelected = false;

                if(pairPointCounter>0)
                {
                    switch (method)
                    {
                    case ETransformationType::Affine:

                        for (int i=0; i<pairPointCounter; i++)
                        {
                            cv::circle(fullWindowReference, cv::Point((float)referencePointsForAffine[i].x/referenceRatioX, (float)referencePointsForAffine[i].y/referenceRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                            cv::circle(fullWindowMoving, cv::Point((float)movingPointsForAffine[i].x/movingRatioX, (float)movingPointsForAffine[i].y/movingRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                        }

                        break;

                    case ETransformationType::Homography:

                        for (int i=0; i<pairPointCounter; i++)
                        {
                            cv::circle(fullWindowReference, cv::Point((float)referencePointsForHomography[i].x/referenceRatioX, (float)referencePointsForHomography[i].y/referenceRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                            cv::circle(fullWindowMoving, cv::Point((float)movingPointsForHomography[i].x/movingRatioX, (float)movingPointsForHomography[i].y/movingRatioY), 3, cv::Scalar(0,255,0), 4,-1);
                        }

                        break;
                    }
                }

                QImage qimgOriginal_PreReference((uchar*)fullWindowReference.data, fullWindowReference.cols, fullWindowReference.rows, fullWindowReference.step, QImage::Format_RGB888);
                ui->lblReferenceImage->setPixmap(QPixmap::fromImage(qimgOriginal_PreReference));
                QImage qimgOriginal_PreMoving((uchar*)fullWindowMoving.data, fullWindowMoving.cols, fullWindowMoving.rows, fullWindowMoving.step, QImage::Format_RGB888);
                ui->lblMovingImage->setPixmap(QPixmap::fromImage(qimgOriginal_PreMoving));
            }
            file.close();
        }
        else
        {
            QMessageBox msgBox;
            msgBox.setText("The text file containing control points cannot be opened!");
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.exec();
        }
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText("Either Reference or Moving image is not loaded properly!");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();
    }
}

void MainUI::on_btnClearControlPoints_clicked()
{
    disableReferenceImageMouseMove = false;
    disableMovingImageMouseMove = false;

    referencePointIsSelected = false;
    movingPointIsSelected = false;

    isRegisterationDone = false;

    pairPointCounter = 0;
    ui->lblPairPointCounter->setText(QString("Selected Pair-Point Counts: " + QString::number(pairPointCounter)));

    movingPointsForHomography.clear();
    referencePointsForHomography.clear();

    for (int i = 0; i<3; i++)
    {
        referencePointsForAffine[i] = cv::Point2f(0.0,0.0);
        movingPointsForAffine[i] = cv::Point2f(0.0,0.0);
    }

    if (isReferenceAvailable)
    {
        cv::Mat fullWindowReference;
        if(isActiveReferenceImageCLAHE)
        {
            cv::Mat CLAHERGB;
            cv::cvtColor(convertedReferenceImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
            cv::resize(CLAHERGB, fullWindowReference, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);
        }
        else
            cv::resize(convertedReferenceImage, fullWindowReference, cv::Size(ui->lblReferenceImage->width(), ui->lblReferenceImage->height()), 0,0,cv::INTER_LINEAR);

        QImage qimg((uchar*)fullWindowReference.data, fullWindowReference.cols,
                    fullWindowReference.rows, fullWindowReference.step, QImage::Format_RGB888);
        ui->lblReferenceImage->setPixmap(QPixmap::fromImage(qimg));
    }

    if (isMovingAvailable)
    {
        cv::Mat fullWindowMoving;
        if(isActiveMovingImageCLAHE)
        {
            cv::Mat CLAHERGB;
            cv::cvtColor(convertedMovingImage_CLAHE, CLAHERGB, CV_GRAY2RGB);
            cv::resize(CLAHERGB, fullWindowMoving, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);
        }
        else
            cv::resize(convertedMovingImage, fullWindowMoving, cv::Size(ui->lblMovingImage->width(), ui->lblMovingImage->height()), 0,0,cv::INTER_LINEAR);

        QImage qimg((uchar*)fullWindowMoving.data, fullWindowMoving.cols,
                    fullWindowMoving.rows, fullWindowMoving.step, QImage::Format_RGB888);
        ui->lblMovingImage->setPixmap(QPixmap::fromImage(qimg));
    }
}

void MainUI::on_btnApplyToAll_clicked()
{
    if (isRegisterationDone)
    {
        seqPath = QFileDialog::getExistingDirectory(this, tr("Select the Directory Containing the Moving and Reference Folders"),
                                                    "",
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);
        if (seqPath == "")
            return;

        QString referenceFolder = QString(referenceImagePath + "/");
        QDir referenceDirectory(referenceFolder);
        QStringList referenceImages = referenceDirectory.entryList(QStringList() << QString("*." + movingImageSuffix),QDir::Files);

        QString movingFolder = QString(movingImagePath + "/");;
        QDir movingDirectory(movingFolder);
        QStringList movingImages = movingDirectory.entryList(QStringList() << QString("*." + movingImageSuffix) ,QDir::Files);

        cv::Mat affineWarpMat(2, 3, CV_32FC1);
        cv::Mat homographyWarpMat(3, 3, CV_32FC1);
        std::vector<cv::Mat> channels;
        cv::Mat imgPair;
        cv::Mat paddedImage;
        cv::Mat matOriginal_Gray;
        cv::Mat warpDst_Gray;
        cv::Mat movingWarpDst;

        int index = 0;

        QString referenceFileName;
        QString movingFileName;
        cv::Mat referenceInput;
        cv::Mat movingInput;

        QDir().mkdir(QString(seqPath + "/Results"));
        QDir().mkdir(QString(seqPath + "/Results/Registered Moving Images"));
        QDir().mkdir(QString(seqPath + "/Results/ImShowPairs"));

        switch (method)
        {
        case ETransformationType::Affine:

            affineWarpMat = cv::getAffineTransform(movingPointsForAffine, referencePointsForAffine);

            while (index < referenceImages.length() && index < movingImages.length())
            {
                referenceFileName = referenceFolder + referenceImages.at(index);
                movingFileName = movingFolder + movingImages.at(index);

                referenceInput = cv::imread(referenceFileName.toLatin1().data());
                movingInput = cv::imread(movingFileName.toLatin1().data());

                qDebug() << movingFileName.toLatin1().data() << movingInput.cols << movingInput.rows;

                cv::warpAffine(movingInput, movingWarpDst, affineWarpMat, movingWarpDst.size());

                cv::imwrite(QString(seqPath + "/Results/Registered Moving Images/"
                                    + QString(movingImages.at(index)).split(".",QString::SkipEmptyParts).at(0)
                                    + "." + movingImageSuffix).toLatin1().data(), movingWarpDst);

                cv::cvtColor(referenceInput, matOriginal_Gray, CV_RGB2GRAY);
                cv::cvtColor(movingWarpDst, warpDst_Gray, CV_RGB2GRAY);

                if(matOriginal_Gray.cols >= movingWarpDst.cols)
                {
                    paddedImage.create(matOriginal_Gray.rows, matOriginal_Gray.cols, CV_8UC1);
                    paddedImage.setTo(0);

                    cv::Mat ROI(paddedImage, cv::Rect(0, 0, movingWarpDst.cols, movingWarpDst.rows));
                    warpDst_Gray.copyTo(ROI);

                    channels.push_back(matOriginal_Gray);
                    channels.push_back(paddedImage);
                    channels.push_back(matOriginal_Gray);

                    merge(channels, imgPair);
                }
                else
                {
                    paddedImage.create(movingWarpDst.rows, movingWarpDst.cols, CV_8UC1);
                    paddedImage.setTo(0);
                    cv::Mat ROI(paddedImage, cv::Rect(0, 0, matOriginal_Gray.cols, matOriginal_Gray.rows));
                    matOriginal_Gray.copyTo(ROI);

                    channels.push_back(warpDst_Gray);
                    channels.push_back(paddedImage);
                    channels.push_back(warpDst_Gray);

                    merge(channels, imgPair);
                }

                cv::imwrite(QString(seqPath + "/Results/ImShowPairs/"
                                    + "ImShowPair_" + QString(movingImages.at(index++)).split(".",QString::SkipEmptyParts).at(0)
                                    + "." + movingImageSuffix).toLatin1().data(), imgPair);

                channels.clear();
            }
            break;
        case ETransformationType::Homography:

            // Find homography
            homographyWarpMat = findHomography(movingPointsForHomography, referencePointsForHomography, cv::RANSAC );//LMEDS,RANSAC,RHO

            while (index < referenceImages.length() && index < movingImages.length())
            {
                referenceFileName = referenceFolder + referenceImages.at(index);
                movingFileName = movingFolder + movingImages.at(index);

                referenceInput = cv::imread(referenceFileName.toLatin1().data());
                movingInput = cv::imread(movingFileName.toLatin1().data());

                // Use homography to warp image
                cv::warpPerspective(movingInput, movingWarpDst, homographyWarpMat, movingWarpDst.size());

                cv::imwrite(QString(seqPath + "/Results/Registered Moving Images/"
                                    + QString(movingImages.at(index)).split(".",QString::SkipEmptyParts).at(0)
                                    + "." + movingImageSuffix).toLatin1().data(), movingWarpDst);

                cv::cvtColor(referenceInput, matOriginal_Gray, CV_RGB2GRAY);
                cv::cvtColor(movingWarpDst, warpDst_Gray, CV_RGB2GRAY);

                if(matOriginal_Gray.cols >= movingWarpDst.cols)
                {
                    paddedImage.create(matOriginal_Gray.rows, matOriginal_Gray.cols, CV_8UC1);
                    paddedImage.setTo(0);

                    cv::Mat ROI(paddedImage, cv::Rect(0, 0, movingWarpDst.cols, movingWarpDst.rows));
                    warpDst_Gray.copyTo(ROI);

                    channels.push_back(matOriginal_Gray);
                    channels.push_back(paddedImage);
                    channels.push_back(matOriginal_Gray);

                    merge(channels, imgPair);
                }
                else
                {
                    paddedImage.create(movingWarpDst.rows, movingWarpDst.cols, CV_8UC1);
                    paddedImage.setTo(0);
                    cv::Mat ROI(paddedImage, cv::Rect(0, 0, matOriginal_Gray.cols, matOriginal_Gray.rows));
                    matOriginal_Gray.copyTo(ROI);

                    channels.push_back(warpDst_Gray);
                    channels.push_back(paddedImage);
                    channels.push_back(warpDst_Gray);

                    merge(channels, imgPair);
                }

                cv::imwrite(QString(seqPath + "/Results/ImShowPairs/"
                                    + "ImShowPair_" + QString(movingImages.at(index++)).split(".",QString::SkipEmptyParts).at(0)
                                    + "." + movingImageSuffix).toLatin1().data(), imgPair);

                channels.clear();
            }
            break;
        }
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText("Registration is not completed yet!\nPlease register a pair of moving-reference images first.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();
    }
}


void MainUI::on_btnRemoveSelectedPoints_clicked()
{
    if (!haveSelectedPair)
    {
        QMessageBox msgBox;
        msgBox.setText("No pair is selected for removal yet.\nFor selecting a pair, double click on a point on either reference or moving images.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();
    }
    else
    {
        if (pairPointCounter > 0)
        {
            switch (method)
            {
            case ETransformationType::Affine:
                if (selectedPairIndex == 0)
                {
                    referencePointsForAffine[0] = referencePointsForAffine[1];
                    referencePointsForAffine[1] = referencePointsForAffine[2];
                    referencePointsForAffine[2] = cv::Point2f(0.0,0.0);

                    movingPointsForAffine[0] = movingPointsForAffine[1];
                    movingPointsForAffine[1] = movingPointsForAffine[2];
                    movingPointsForAffine[2] = cv::Point2f(0.0,0.0);
                }
                else if (selectedPairIndex == 1)
                {
                    referencePointsForAffine[1] = referencePointsForAffine[2];
                    referencePointsForAffine[2] = cv::Point2f(0.0,0.0);

                    movingPointsForAffine[1] = movingPointsForAffine[2];
                    movingPointsForAffine[2] = cv::Point2f(0.0,0.0);
                }
                else if (selectedPairIndex == 2)
                {
                    referencePointsForAffine[2] = cv::Point2f(0.0,0.0);

                    movingPointsForAffine[2] = cv::Point2f(0.0,0.0);
                }

                break;
            case ETransformationType::Homography:
                referencePointsForHomography.erase(referencePointsForHomography.begin() + selectedPairIndex);
                movingPointsForHomography.erase(movingPointsForHomography.begin() + selectedPairIndex);

                break;
            }

            pairPointCounter--;

            selectedPairIndex = -1;
            haveSelectedPair = false;

            updateReferenceImage();
            updateMovingImage();
        }
    }
}
