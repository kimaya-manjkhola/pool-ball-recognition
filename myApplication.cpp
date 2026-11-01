#include <opencv2/opencv.hpp>
#include <iostream>
#include <filesystem>
#include <string>
using namespace cv;
using namespace std;

// FIXED INFORMATION FROM THE ASSIGNMENT --> it is already given
const int TABLE_ROWS = 683; //cushion to cushion = 683 mm -> pixel = 1mm
const int TABLE_COLS = 1434; // cushion to cushion = 1434 mm
const int NUM_IMAGES = 7;
const int NUM_BALLS = 16; // index 0..14 = balls 1..15, index 15 = white ball
const string IMAGE_NAMES[NUM_IMAGES] = { "AllBalls", "Table1", "Table2","Table3", "Table4", "Table5", "Table6" };

//Ground truth {rows, column} in the transformed table image. {-1,-1} = ball not on the table
//Copied from Ball_Locations_In_Trnasformed_Image.xlsx.
//CORRECTIONS (Table1): balls 2, 4 and 11 had row and column swapped in the spreadsheet
//(rows > 683 are impossible). Ball 3 was listed at the same place as ball 7; its position
//was estimated from the rack spacing.
const int GROUND_TRUTH[NUM_IMAGES] [NUM_BALLS] [2] = {
    { {253,908}, {250,959}, {255,1013}, {256,1072}, {254,1132}, {262,1184}, {256,1244}, {253,1307}, {329,901}, {330,961}, {334,1130}, {335,1013}, {336,1067}, {342,1245}, {345,1187}, {293,797} }, // AllBalls
    { {309,1092}, {429,1212}, {381,1132}, {335,1050}, {285,1211}, {261,1170}, {352,1173}, {331,1132}, {240,1210}, {310,1170}, {407,1169}, {281,1135}, {374,1212}, {360,1091}, {331,1213}, {288,29} }, // Table1 (corrected)
    { {282,1103}, {620,1224}, {391,1121}, {310,1305}, {315,1046}, {202,621}, {361,1173}, {331,1132}, {177,725}, {185,1269}, {665,798}, {296,438}, {53,1235}, {362,1090}, {399,1205}, {507,1169} }, // Table2
    { {23,908}, {616,1224}, {447,1199}, {336,29}, {305,1406}, {210,621}, {260,1294}, {226,1252}, {177,725}, {171,1270}, {665,798}, {249,351}, {53,1233}, {169,901}, {399,1201}, {434,29} }, // Table3
    { {23,908}, {546,1407}, {444,1204}, {-1,-1}, {305,1406}, {210,621}, {260,1294}, {226,1252}, {181,724}, {171,1270}, {665,798}, {249,351}, {58,1231}, {169,901}, {399,1201}, {88,1392} }, // Table4
    { {535,23}, {546,1407}, {444,1204}, {-1,-1}, {305,1406}, {131,25}, {260,1294}, {226,1252}, {538,72}, {171,1270}, {665,798}, {249,351}, {-1,-1}, {169,901}, {399,1201}, {608,26} }, // Table5
    { {535,23}, {477,1364}, {444,1204}, {-1,-1}, {305,1406}, {131,25}, {260,1294}, {226,1252}, {538,72}, {171,1270}, {-1,-1}, {249,351}, {-1,-1}, {169,901}, {399,1201}, {629,947} } // Table6
};

string BallName(int b) { return (b == 15) ? "W": to_string(b + 1); }

// STAGE 0 ---> Perspective Transformation
// Solves for the 8 perspective coefficients from 4 point pairs:
//   u = (a*x + b*y + c) / (g*x + h*y + 1)
//   v = (d*x + e*y + f) / (g*x + h*y + 1)
// Each corner pair gives 2 linear equations -> 4 pairs give 8 equations for 8 unknowns.
Mat PerspectiveFromCorners(const Point2f from[4], const Point2f to[4])
{
    Mat A(8, 8, CV_64F, Scalar(0));
    Mat rhs(8, 1, CV_64F);
    for (int i = 0; i < 4; i++)
    {
        double x = from[i].x, y = from[i].y, u = to[i].x, v = to[i].y;
        double rowU[8] = { x, y, 1, 0, 0, 0, -x * u, -y * u };
        double rowV[8] = { 0, 0, 0, x, y, 1, -x * v, -y * v };
        for (int j = 0; j < 8; j++)
        {
            A.at<double>(2 * i, j)     = rowU[j];
            A.at<double>(2 * i + 1, j) = rowV[j];
        }
        rhs.at<double>(2 * i)     = u;
        rhs.at<double>(2 * i + 1) = v;
    }
    Mat coeffs;
    solve(A, rhs, coeffs);                 // solve the 8 x 8 linear system
    Mat transform(3, 3, CV_64F);
    for (int k = 0; k < 8; k++) transform.at<double>(k / 3, k % 3) = coeffs.at<double>(k);
    transform.at<double>(2, 2) = 1.0;
    return transform;
}

// Maps the 4 table corners in the photo onto the corners of a 1434 x 683 rectangle.
Mat WarpToTable(const Mat& photo)
{
    // Corners from the brief, written as (x = column, y = row): TL, TR, BR, BL
    Point2f photoCorners[4] = { Point2f(337, 35), Point2f(1570, 21), Point2f(1888, 451), Point2f(38, 472) };
    Point2f tableCorners[4] = { Point2f(0, 0), Point2f(TABLE_COLS - 1, 0),
                                Point2f(TABLE_COLS - 1, TABLE_ROWS - 1), Point2f(0, TABLE_ROWS - 1) };
    Mat transform = PerspectiveFromCorners(photoCorners, tableCorners);
    Mat table;
    warpPerspective(photo, table, transform, Size(TABLE_COLS, TABLE_ROWS), INTER_LINEAR);   // bilinear interpolation
    return table;
}

//Draws the ground-truth positions so we can check the transform lines up
void DrawGroundTruth(Mat& table, int imageIndex)
{
    for (int b = 0; b < NUM_BALLS; b++)
    {
        int row = GROUND_TRUTH[imageIndex][b][0];
        int col = GROUND_TRUTH[imageIndex][b][1];
        if (row < 0) continue; // ball not on the table
        circle(table, Point(col, row), 21, Scalar(0, 0, 255), 2); // 21 px = ball radius (43 mm / 2)
        putText(table, BallName(b), Point(col + 22, row - 22), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0,0,255), 2);
    }
}

void MyApplication()
{
    filesystem::create_directories("Output");
    for (int i = 0; i < NUM_IMAGES; i++)
    {
        Mat photo = imread("Media/" + IMAGE_NAMES[i] + ".jpeg");
        if (photo.empty()) {cout << "Could not load " << IMAGE_NAMES[i] << endl; continue; }

        Mat table = WarpToTable(photo);
        imwrite("Output/" + IMAGE_NAMES[i] + "_1_warped.png", table);

        Mat check = table.clone();
        DrawGroundTruth(check, i);
        imwrite("Output/" + IMAGE_NAMES[i] + "_2_groundtruth.png", check);

        cout << IMAGE_NAMES[i] << ": warped to " << table.cols << " x " << table.rows << endl;
    }
    cout << "Done - check the output folder" << endl;
}