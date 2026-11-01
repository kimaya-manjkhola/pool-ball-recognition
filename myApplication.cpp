#include <opencv2/opencv.hpp>
#include <iostream>
using namespace cv;
using namespace std;

void MyApplication()
{
    Mat img = imread("Media/Table1.jpeg");
    if (img.empty()) { cout << "Could not load image - check the Media folder" << endl; return; }
    cout << "Loaded Table1: " << img.cols << " x " << img.rows << endl;
    imshow("Table1", img);
    waitKey(0);
}