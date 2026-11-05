#include <opencv2/opencv.hpp>
#include <iostream>
#include <filesystem>
#include <string>
#include <algorithm>
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
// AllBalls: balls 11-15 were labelled in the wrong order (checked against the numbers on the balls).
// Table2: balls 4 and 5 had swapped positions.
const int GROUND_TRUTH[NUM_IMAGES] [NUM_BALLS] [2] = {
    { {253,908}, {250,959}, {255,1013}, {256,1072}, {254,1132}, {262,1184}, {256,1244}, {253,1307}, {329,901}, {330,961}, {335,1013}, {336,1067}, {334,1130}, {345,1187}, {342,1245}, {293,797} },  // AllBalls (corrected: 11-15 reordered)
    { {309,1092}, {429,1212}, {381,1132}, {335,1050}, {285,1211}, {261,1170}, {352,1173}, {331,1132}, {240,1210}, {310,1170}, {407,1169}, {281,1135}, {374,1212}, {360,1091}, {331,1213}, {288,29} }, // Table1 (corrected)
    { {282,1103}, {620,1224}, {391,1121}, {315,1046}, {310,1305}, {202,621}, {361,1173}, {331,1132}, {177,725}, {185,1269}, {665,798}, {296,438}, {53,1235}, {362,1090}, {399,1205}, {507,1169} },  // Table2 (corrected: 4 and 5 swapped)
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

//STAGE 1 --> Find the balls
const double BALL_RADIUS_MM = 21.5; //ball diameter is 43 mm (from the brief)
const int BACKGROUND_KERNEL = 151; //median window, several ball diameters wide
const Point PHOTO_CORNERS[4] = { Point(337, 35), Point(1570, 21), Point(1888, 451), Point(38, 472) }; // TL, TR, BR, BL

struct Ball
{
    Point2f photo; // centre in the original photo (x = column, y = row)
    float radius; // radius in the photo, in pixels
    Point2f table; // centre in table coordinates (x = column, y = row), 1 unit = 1mm
};

// Transform from the photo to table coordinates (same corners as WarpToTable)
Mat TableTransform()
{
    Point2f photoCorners[4], tableCorners[4] = { Point2f (0, 0), Point2f(TABLE_COLS - 1, 0), Point2f(TABLE_COLS - 1, TABLE_ROWS - 1), Point2f(0, TABLE_ROWS - 1)  };
    for (int i = 0; i < 4; i++) photoCorners[i] = Point2f(PHOTO_CORNERS[i]);
    return PerspectiveFromCorners(photoCorners, tableCorners);
}

// Applies a perspective transform to a single point
Point2f TransformPoint(const Point2f& point, const Mat& transform)
{
    vector<Point2f> in = { point }, out;
    perspectiveTransform(in, out, transform);
    return out[0];
}

//Median filter with a mirrored border, so balls near the image edge are not copied into the background
Mat LargeMedian(const Mat& channel, int kernel)
{
    int pad = kernel / 2;
    Mat padded, smoothed;
    copyMakeBorder(channel, padded, pad, pad, pad, pad, BORDER_REFLECT);
    medianBlur(padded, smoothed, kernel);
    return smoothed (Rect(pad, pad, channel.cols, channel.rows)).clone();
}

// Colour difference between each pixel and the local cloth colour (L*a*b* colour space).
// The cloth is the most common colour in any large window, so a large mdian filter estimates it.
Mat ColourDifference(const Mat& photo)
{
    Mat lab;
    cvtColor(photo, lab, COLOR_BGR2Lab);
    vector<Mat> channels;
    split(lab, channels);
    Mat sumSquares = Mat::zeros(photo.size(), CV_32F);
    for (int c = 0; c < 3; c++)
    {
        Mat value, background;
        channels[c].convertTo(value, CV_32F);
        LargeMedian(channels[c], BACKGROUND_KERNEL).convertTo(background, CV_32F);
        Mat difference = value - background;
        sumSquares += difference.mul(difference);
    }
    Mat distance, result;
    sqrt(sumSquares, distance);
    distance.convertTo(result, CV_8U, 3.0); //scale x3 for contrast (values above 255 saturate)
    return result;
}

vector<Ball> FindBalls(const Mat& photo, const string& name)
{
    Mat toTable = TableTransform();
    Mat toPhoto = toTable.inv();

    //1. Colour difference from the cloth, restricted to the table area
    Mat difference = ColourDifference(photo);
    Mat tableMask = Mat::zeros(photo.size(), CV_8U);
    fillConvexPoly(tableMask, PHOTO_CORNERS, 4, Scalar(255));
    Mat masked;
    bitwise_and(difference, tableMask, masked);
    imwrite("Output/" + name + "_3_difference.png", masked);

    //2. Otsu thresholding --> binary "not cloth" image
    Mat foreground;
    threshold(masked, foreground, 0, 255, THRESH_BINARY | THRESH_OTSU);
    imwrite("Output/" + name + "_4_foreground.png", foreground);

    //3. Hough transform for circles on the (smoothed) difference image
    Mat smoothed;
    GaussianBlur(masked, smoothed, Size(5, 5), 1.5);
    vector<Vec3f> circles;
    HoughCircles(smoothed, circles, HOUGH_GRADIENT, 1, 20, 100, 15, 14, 30);
    //
    //
    //

    //4. Verify each circle
    Point2f pockets[6] = { Point2f(0, 0), Point2f(TABLE_COLS / 2.0f, 0), Point2f(TABLE_COLS, 0), Point2f(0, TABLE_ROWS), Point2f(TABLE_COLS / 2.0f, TABLE_ROWS), Point2f(TABLE_COLS, TABLE_ROWS) };
    Mat candidatesImage = photo.clone(), detectedImage = photo.clone();
    vector<Ball> balls;
    for (const Vec3f& c : circles)
    {
        Point2f centre(c[0], c[1]);
        float radius = c[2];
        circle(candidatesImage, centre, cvRound(radius), Scalar(0, 255, 255), 2);

        //(a) at least half of the circle must be "not cloth"
        Mat disc = Mat::zeros(photo.size(), CV_8U);
        circle(disc, centre, cvRound(radius), Scalar(255), FILLED);
        if (mean(foreground, disc)[0] / 255.0 < 0.5) continue;

        //(b) centree must be on the table and not in a pocket (within 1.5 ball diameters of a pocket)
        Point2f t = TransformPoint(centre, toTable);
        if (t.x < 0 || t.x >= TABLE_COLS || t.y < 0 || t.y >= TABLE_ROWS) continue;
        bool inPocket = false;
        for (const Point2f& p : pockets) if (norm(t - p) < 1.5 * 2 * BALL_RADIUS_MM) inPocket = true;
        if (inPocket) continue;

        //(c) radius must match the expected size of a 43 mm ball at that point on the table (within a third)
        Point2f left = TransformPoint(Point2f(t.x - BALL_RADIUS_MM, t.y), toPhoto);
        Point2f right = TransformPoint (Point2f(t.x + BALL_RADIUS_MM, t.y), toPhoto);
        double expectedRadius = norm(right - left) / 2.0;
        double ratio = radius / expectedRadius;
        if (ratio < 0.75 || ratio > 1.33) continue;

        balls.push_back({ centre, radius, t });
        circle(detectedImage, centre, cvRound(radius), Scalar(0, 0, 255), 2);

    }
    imwrite("Output/" + name + "_5_candidates.png", candidatesImage);
    imwrite("Output/" + name + "_6_detected.png", detectedImage);
    return balls;
}

//STAGE 1 --> Evaluation
struct DetectionScore
{
    int tp = 0, fp = 0, fn = 0; // true positives, false positives, false negatives
    double errorSum = 0, errorMax = 0; // position error of the true positives, in mm
};

//Matches detections to the ground truth: closest pairs first, each ball used at most once,
//and a pair only counts if the centres are within one ball radius (21.5 mm).
//Returns, for each detection, the index of the ground-truth ball it matched (-1 = false positive).
vector<int> MatchToGroundTruth(const vector<Ball>& balls, int imageIndex, DetectionScore& score)
{
    struct Pair { double distance; int detection, ball; };
    vector<Pair> pairs;
    int ballsOnTable = 0;
    for (int b = 0; b < NUM_BALLS; b++)
    {
        int row = GROUND_TRUTH[imageIndex][b][0], col = GROUND_TRUTH[imageIndex][b][1];
        if (row < 0) continue;
        ballsOnTable++;
        for (int d =0; d < (int)balls.size(); d++)
        {
            double distance = norm(balls[d].table - Point2f((float)col, (float)row));
            if (distance <= BALL_RADIUS_MM) pairs.push_back({ distance, d, b });
        }
    }
    sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) { return a.distance < b.distance; });
    
    vector<int> matchOf(balls.size(), -1);
    vector<bool> ballUsed(NUM_BALLS, false);
    for (const Pair& p : pairs)
    {
        if (matchOf[p.detection] != -1 || ballUsed[p.ball]) continue;
        matchOf[p.detection] = p.ball;
        ballUsed[p.ball] = true;
        score.tp++;
        score.errorSum += p.distance;
        score.errorMax = max(score.errorMax, p.distance);
    }
    score.fp = (int)balls.size() - score.tp;
    score.fn = ballsOnTable - score.tp;
    return matchOf;
}

//Green = correct detecrion, red cross = false positive, blue circle = missed ball
void DrawEvaluation(Mat& table, const vector<Ball>& balls, const vector<int>& matchOf, int imageIndex)
{
    vector<bool> found(NUM_BALLS, false);
    for (int d = 0; d < (int)balls.size(); d++)
    {
        if (matchOf[d] >= 0) { found[matchOf[d]] = true; circle(table, balls[d].table, 21, Scalar(0, 255,0), 3); }
        else drawMarker(table, balls[d].table, Scalar(0, 0, 255), MARKER_TILTED_CROSS, 30, 4);
    }
    for (int b = 0; b < NUM_BALLS; b++)
    {
        int row = GROUND_TRUTH[imageIndex][b][0], col = GROUND_TRUTH[imageIndex][b][1];
        if (row < 0 || found[b]) continue;
        circle(table, Point(col, row), 24, Scalar(255, 0, 0), 3);
        putText(table, "missed " + BallName(b), Point(col + 26, row), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 0, 0), 2);
    }
}

void PrintDetectionScore(const string& label, const DetectionScore& s)
{
    double precision = (s.tp + s.fp > 0) ? (double)s.tp / (s.tp + s.fp) : 0;
    double recall = (s.tp + s.fn > 0) ? (double)s.tp / (s.tp + s.fn) : 0;
    double f1 = (precision + recall > 0) ? 2 * precision * recall / (precision + recall) : 0;
    double meanError = (s.tp > 0) ? s.errorSum / s.tp : 0;
    cout << format("%-8s TP=%2d FP=%2d FN=%2d Precision=%.3f Recall=%.3f F1=%.3f MeanError=%.1fmm MaxError=%.1fmm", label.c_str(), s.tp, s.fp, s.fn, precision, recall, f1, meanError, s.errorMax) << endl;
}

//STAGE 2 ---> Recognise the Balls 
// Expected radius (in photo pixels) of a 43 mm ball centred at a point on the table
double ExpectedRadius(const Point2f& tablePoint, const Mat& toPhoto)
{
    Point2f left  = TransformPoint(Point2f(tablePoint.x - BALL_RADIUS_MM, tablePoint.y), toPhoto);
    Point2f right = TransformPoint(Point2f(tablePoint.x + BALL_RADIUS_MM, tablePoint.y), toPhoto);
    return norm(right - left) / 2.0;
}

// 3D colour histogram (18 hue x 4 saturation x 4 value bins) of the pixels inside a disc, normalised to sum to 1
Mat BallHistogram(const Mat& hsv, const Point2f& centre, double radius)
{
    Mat disc = Mat::zeros(hsv.size(), CV_8U);
    circle(disc, centre, cvRound(radius), Scalar(255), FILLED);
    int channels[3] = { 0, 1, 2 };
    int bins[3] = { 18, 4, 4 };
    float hueRange[2] = { 0, 180 }, satRange[2] = { 0, 256 }, valRange[2] = { 0, 256 };
    const float* ranges[3] = { hueRange, satRange, valRange };
    Mat histogram;
    calcHist(&hsv, 1, channels, disc, histogram, 3, bins, ranges);
    normalize(histogram, histogram, 1, 0, NORM_L1);
    return histogram;
}

// Training: one histogram per ball, taken from AllBalls.jpeg at the ground-truth positions
vector<Mat> TrainBallModels()
{
    Mat photo = imread("Media/AllBalls.jpeg"), hsv;
    cvtColor(photo, hsv, COLOR_BGR2HSV);
    Mat toPhoto = TableTransform().inv();
    vector<Mat> models;
    for (int b = 0; b < NUM_BALLS; b++)
    {
        Point2f tablePoint((float)GROUND_TRUTH[0][b][1], (float)GROUND_TRUTH[0][b][0]);
        Point2f centre = TransformPoint(tablePoint, toPhoto);
        models.push_back(BallHistogram(hsv, centre, ExpectedRadius(tablePoint, toPhoto)));
    }
    return models;
}

// Compares every detected ball with every model (Bhattacharyya distance), then assigns labels
// best match first, using each ball label at most once (there is only one of each ball).
// Returns the label (0..15) given to each detection.
vector<int> RecogniseBalls(const Mat& photo, const vector<Ball>& balls, const vector<Mat>& models)
{
    Mat hsv;
    cvtColor(photo, hsv, COLOR_BGR2HSV);
    Mat toPhoto = TableTransform().inv();
    struct Pair { double distance; int detection, label; };
    vector<Pair> pairs;
    for (int d = 0; d < (int)balls.size(); d++)
    {
        Mat histogram = BallHistogram(hsv, balls[d].photo, ExpectedRadius(balls[d].table, toPhoto));
        for (int b = 0; b < NUM_BALLS; b++)
            pairs.push_back({ compareHist(histogram, models[b], HISTCMP_BHATTACHARYYA), d, b });
    }
    sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) { return a.distance < b.distance; });

    vector<int> labelOf(balls.size(), -1);
    vector<bool> labelUsed(NUM_BALLS, false);
    for (const Pair& p : pairs)
    {
        if (labelOf[p.detection] != -1 || labelUsed[p.label]) continue;
        labelOf[p.detection] = p.label;
        labelUsed[p.label] = true;
    }
    return labelOf;
}

// Writes the recognised label on each detection: green = correct, red = wrong, grey = false positive
void DrawRecognition(Mat& table, const vector<Ball>& balls, const vector<int>& matchOf, const vector<int>& labelOf)
{
    for (int d = 0; d < (int)balls.size(); d++)
    {
        Scalar colour = (matchOf[d] < 0) ? Scalar(128, 128, 128) : (labelOf[d] == matchOf[d]) ? Scalar(0, 255, 0) : Scalar(0, 0, 255);
        string text = BallName(labelOf[d]);
        if (matchOf[d] >= 0 && labelOf[d] != matchOf[d]) text += " (is " + BallName(matchOf[d]) + ")";
        circle(table, balls[d].table, 21, colour, 3);
        putText(table, text, balls[d].table + Point2f(24, 8), FONT_HERSHEY_SIMPLEX, 0.8, colour, 2);
    }
}

// Rows = true ball, columns = recognised ball
void PrintConfusionMatrix(const int confusion[NUM_BALLS][NUM_BALLS])
{
    cout << "Confusion matrix (rows = true ball, columns = recognised as):" << endl << "     ";
    for (int c = 0; c < NUM_BALLS; c++) cout << format("%3s", BallName(c).c_str());
    cout << endl;
    for (int r = 0; r < NUM_BALLS; r++)
    {
        cout << format("%4s ", BallName(r).c_str());
        for (int c = 0; c < NUM_BALLS; c++) cout << format("%3d", confusion[r][c]);
        cout << endl;
    }
}

void MyApplication()
{
    filesystem::create_directories("Output");
    DetectionScore total;
    vector<Mat> models = TrainBallModels();             // Stage 2 training (AllBalls.jpeg only)
    int confusion[NUM_BALLS][NUM_BALLS] = {};
    int recognisedCorrectly = 0, recognisedTotal = 0, ballsOnTables = 0;

    for (int i = 0; i < NUM_IMAGES; i++)
    {
        Mat photo = imread("Media/" + IMAGE_NAMES[i] + ".jpeg");
        if (photo.empty()) { cout << "Could not load " << IMAGE_NAMES[i] << endl; continue; }

        // Stage 0: flatten the table
        Mat table = WarpToTable(photo);
        imwrite("Output/" + IMAGE_NAMES[i] + "_1_warped.png", table);
        Mat check = table.clone();
        DrawGroundTruth(check, i);
        imwrite("Output/" + IMAGE_NAMES[i] + "_2_groundtruth.png", check);

        // Stage 1: find the balls
        vector<Ball> balls = FindBalls(photo, IMAGE_NAMES[i]);
        for (const Ball& b : balls)
            circle(check, b.table, 6, Scalar(0, 255, 0), FILLED);
        imwrite("Output/" + IMAGE_NAMES[i] + "_7_detected_table.png", check);

        // Stage 1 evaluation (AllBalls is the training image, so it is not part of the test results)
        DetectionScore score;
        vector<int> matchOf = MatchToGroundTruth(balls, i, score);
        Mat evaluation = table.clone();
        DrawEvaluation(evaluation, balls, matchOf, i);
        imwrite("Output/" + IMAGE_NAMES[i] + "_8_evaluation.png", evaluation);
        PrintDetectionScore(IMAGE_NAMES[i], score);

        // Stage 2: recognise the balls
        vector<int> labelOf = RecogniseBalls(photo, balls, models);
        Mat recognition = table.clone();
        DrawRecognition(recognition, balls, matchOf, labelOf);
        imwrite("Output/" + IMAGE_NAMES[i] + "_9_recognition.png", recognition);

        if (i > 0)
        {
            total.tp += score.tp; total.fp += score.fp; total.fn += score.fn;
            total.errorSum += score.errorSum; total.errorMax = max(total.errorMax, score.errorMax);
            ballsOnTables += score.tp + score.fn;
            // Stage 2 evaluation: only correctly located balls have a true label to compare with
            for (int d = 0; d < (int)balls.size(); d++)
            {
                if (matchOf[d] < 0) continue;
                confusion[matchOf[d]][labelOf[d]]++;
                recognisedTotal++;
                if (labelOf[d] == matchOf[d]) recognisedCorrectly++;
            }
        }
    }
    cout << "---- Stage 1 results over the 6 test images ----" << endl;
    PrintDetectionScore("TOTAL", total);
    cout << "---- Stage 2 results over the 6 test images ----" << endl;
    cout << format("Recognition accuracy (of correctly located balls) = %d / %d = %.3f",
                   recognisedCorrectly, recognisedTotal, (double)recognisedCorrectly / recognisedTotal) << endl;
    cout << format("Located AND recognised correctly (of all balls)    = %d / %d = %.3f",
                   recognisedCorrectly, ballsOnTables, (double)recognisedCorrectly / ballsOnTables) << endl;
    PrintConfusionMatrix(confusion);
    cout << "Done - check the Output folder" << endl;
}