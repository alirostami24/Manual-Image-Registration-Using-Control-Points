#ifndef MAINUI_H
#define MAINUI_H

#include <QMainWindow>
#include <QFileDialog>
#include <QDebug>
#include <QMouseEvent>
#include <QMessageBox>
#include <qmath.h>

#include <algorithm>
#include <fstream>

#include "Dialog.h"

#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/calib3d/calib3d.hpp"

namespace Ui {
class MainUI;
}

class MainUI : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainUI(QWidget *parent = 0);
    ~MainUI();

private Q_SLOTS:
    void on_btnRegister_clicked();

    void on_btnLoadReference_clicked();

    void on_btnLoadMoving_clicked();

     bool eventFilter( QObject *dist, QEvent *event );

     void on_btnSavePoint_clicked();

     void on_btnRemoveCurrentPoints_clicked();

     void on_btnSaveRegistered_clicked();

     void on_btnClear_clicked();

     void on_dsbReferenceX_valueChanged(double arg1);

     void on_dsbReferenceY_valueChanged(double arg1);

     void on_dsbMovingX_valueChanged(double arg1);

     void on_dsbMovingY_valueChanged(double arg1);

     void on_btnSaveImshowPair_clicked();

     void on_actionAffine_triggered();

     void on_actionHomography_triggered();

     void on_chbMovingContrastEnhancement_toggled(bool checked);

     void on_btnSaveControlPoints_clicked();

     void on_btnLoadControlPoints_clicked();

     void on_btnClearControlPoints_clicked();

     void on_btnApplyToAll_clicked();

     void on_chbReferenceContrastEnhancement_toggled(bool checked);

     void sltReceiveOkForControlPointRemoval();

     void on_btnRemoveSelectedPoints_clicked();

private:
    Ui::MainUI *ui;

    /* converted version of the reference image from BGR to RGB */
    cv::Mat convertedReferenceImage;

    /* converted version of the moving image from BGR to RGB */
    cv::Mat convertedMovingImage;

    /* an image of size: zoomFactor*(imageSize)
    * This is defined to do rect zoom easierfor reference image
    */
    cv::Mat enlargedReference;

    /*
     * an image of size: zoomFactor*(imageSize)
     * This is defined to do rect zoom easierfor moving image
    */
    cv::Mat enlargedMoving;

    /*
     * images for keeping the converted version of the loaded image
    */
    cv::Mat convertedMovingImage_CLAHE;
    cv::Mat convertedReferenceImage_CLAHE;

    /*
     * QPointF structure for keeping the clicked points in moving and reference images
    */
    QPointF pReference;
    QPointF pMoving;

    /*
     * Some boolean variable for controlling user actions and preventing exceptions
    */
    bool disableReferenceImageMouseMove;
    bool disableMovingImageMouseMove;
    bool referencePointIsSelected;
    bool movingPointIsSelected;
    bool isRegisterationDone;
    bool isActiveMovingImageCLAHE;
    bool isActiveReferenceImageCLAHE;

    /* A path in which there are images whose transformation matrix is the same as the one registered manuaaly */
    QString seqPath;
    QString movingImagePath;
    QString referenceImagePath;

    /*
     * The suffix of the moving image is used for the writing of results.
     *  The format of the moving image and the results are the same.
    */
    QString movingImageSuffix;

    /*
     * this enum is needed in deciding on each method of transformation is selected by the user
    */
    enum ETransformationType {Affine, Homography };
    int method;

    /*
     * A mat which is used for the result of the warping operation.
    */
    cv::Mat warpDst;

    /*
     * This mat is used for saving imShowPair image as it is defined by MATLAB.
    */
    cv::Mat imShowPair;

    /*
     * The following variables are used for keeping the saved pair points.
    */
    cv::Point2f movingPointsForAffine[3];
    cv::Point2f referencePointsForAffine[3];
    std::vector<cv::Point2f> referencePointsForHomography;
    std::vector<cv::Point2f> movingPointsForHomography;

    /*
     * indicates the total pair number of the saved points.
    */
    int pairPointCounter;

    /*
     * The following variables are used for keeping the correlation between the UI and real coordinates.
    */
    float referenceRatioX;
    float referenceRatioY;
    float movingRatioX;
    float movingRatioY;

    /*
     * The following variables are used for handling the removal of the pair points.
    */
    bool haveSelectedPair;
    int selectedPairIndex;

    /*
     * The following variables are used for making sure that moving
     * and reference images are loaded properly or not..
    */
    bool isReferenceAvailable;
    bool isMovingAvailable;

    /*
     * This variable controls the amount of the zoom needed by the user to click on the
     * corresponding points.
    */
    int zoomFactor;

    CDialog *controlPointDialog;

    /*
     * The following functions are implemented to update the images displayed on the UI
     * when the user is interacting with the UI.
    */
    void plotReferenceZoomedImage();
    void plotMovingZoomedImage();
    void drawReferenceZoomed();
    void drawMovingZoomed();
    void updateReferenceImage();
    void updateMovingImage();
};




#endif // MAINUI_H
