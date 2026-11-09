# Development history
This folder documents how the project was developed before the GitHub repository existed (development started on 1 Nov 2026; the repository was created on 5 Nov 2026).

The `reconstructed-history` branch shows how the code developed from 1 to 4 November 2026, before this repository was created on 5 November. Each commit there is labelled [Reconstructed]:
it was recorded on 9 Nov 2026, from the code as it was on the date below.

| Commit | Written (author date) | Version |
|---|---|---|
| 193a3cc | 1 Nov 2026, 11:35 | Project setup: test program loads and displays Table1 |
| 54a192b | 1 Nov 2026, 16:12 | Stage 0: perspective transform and ground-truth overlay |
| 8b1937f | 2 Nov 2026, 15:21 | Stage 1: ball detection |
| 7f197b2 | 4 Nov 2026, 19:06 | Stage 1 evaluation: first version (does not compile yet) |
| 731c039 | 4 Nov 2026, 19:14 | Stage 1 evaluation: compile errors fixed |

Development continues on `main` from commit 1f2b20e (Stage 2 recognition, 5 Nov 2026).

## Snapshots
These are exact, unchanged copies of myApplication.cpp at two points during development.

| File | Saved | State of the code |
|---|---|---|
| 2026-11-01_stage0_perspective_transform.cpp | 1 Nov 2026, ~16:12 | Stage 0 working: perspective transform to table coordinates and ground-truth overlay (with Table1 ground-truth corrections) |
| 2026-11-04_stage1_detection_and_evaluation_before_fixes.cpp | 4 Nov 2026, ~19:06 | Stage 1 ball detection plus the first version of the evaluation code, before I fixed its compile errors |

The final version (Stage 2 recognition added, 4-5 Nov) is myApplication.cpp,
committed in 1f2b20e ("Stages 0-2 working...").

## Development timeline
### Sunday 1 November 2026: setup, Stage 0, detection approach
- Read the assignment brief and planned the stages: perspective transform, find balls,
  evaluate, recognise balls, evaluate, report.
- Set up the Mac: Xcode command line tools, Homebrew, OpenCV 5 and CMake, and the C/C++
  extension in VS Code. Anaconda's older cmake was found first, so I used Homebrew's cmake
  by its full path (/opt/homebrew/bin/cmake).
- Created the project (CMakeLists.txt, main.cpp, myApplication.cpp, Media folder) and a test
  program that loads and displays Table1.jpeg (1931 x 509 pixels).
- Went through the lecture slides to choose methods covered in the module.
- Stage 0: perspective transformation from the 4 given corners to a 1434 x 683 table image
  (1 pixel = 1 mm), with bilinear interpolation. With OpenCV 5.0 (installed via Homebrew),
  the build failed with "use of undeclared identifier 'getPerspectiveTransform'", so I
  computed the transform myself by solving the 8 x 8 linear system from the 4 corner pairs
  (Geometric slides 11-13).
- Put the ground truth into the code and drew it on the warped images to check the transform.
  Found errors in the Table1 ground truth (see "Ground-truth corrections" below).

### Monday 2 November 2026: Stage 1, finding the balls
- Balls appear as ovals in the warped image but as circles in the photo, so detection runs
  on the photo and the ball centres are converted to table coordinates.
- Method: a large median filter (151 px) estimates the local cloth colour; each pixel's
  L*a*b* colour difference from it is thresholded with Otsu; the Hough transform for circles
  finds ball-sized circles; each circle is kept only if at least half of it is not cloth,
  it is on the table and not in a pocket, and its size matches a 43 mm ball at that point.
- Fixed typing errors that stopped it compiling. Saved images of every stage to Output.

### Wednesday 4 November 2026: Stage 1 evaluation and Stage 2 recognition
- Added automatic evaluation: one-to-one matching of detections to ground truth within
  21.5 mm (one ball radius), giving TP, FP, FN, precision, recall, F1 and position error.
  Fixed typing errors, including one in the precision formula.
- Stage 1 results on the 6 test images: precision 0.966, recall 0.944, F1 0.955,
  mean position error 5.8 mm.
- Stage 2: an HSV colour histogram (18 x 4 x 4 bins) of each ball from AllBalls.jpeg only,
  compared using the Bhattacharyya distance, with each ball label used once.
- Found further ground-truth errors in AllBalls and Table2 (see below).

### Thursday 5 November 2026: results, GitHub, report planning
- Full results: recognition accuracy 81/85 (95.3%); located and recognised correctly
  81/90 (90.0%). Confusion matrix errors: 1 and 9 swapped once, 14 recognised as 6 twice.
- Created the private GitHub repository and pushed the code (commit 1f2b20e).

### Friday 6 November 2026
- VS Code's local history only keeps the most recent saves, so earlier versions were no
  longer available there. Added the two snapshots above to this folder, to show I've been working on this project from long before.

### Saturday 7 November 2026
- Committed the snapshots and this README.
- Added a lighting robustnes test: detection and cognition re-run on the test images 
  with brightness scaled by 0.5, 0.75, 1.0, 1.25, and 1.5, models still trained on the original trained on the original AllBalls image. Detection stayed robust (F1 0.94 - 0.96) because the cloth model and the OTsu threshold adapt to each image. Recognition dropped to 53 - 62% at the extremes because the colour models include absolute brightness and come from one training image.

## Ground-truth corrections
The assignment released an updated spreadsheet correcting 4 ball positions (Table1 white
ball; ball 2 in Tables 4, 5 and 6). This updated version was used throughout.

While checking the ground truth against the images, I found further errors not covered
by that update. Each was checked against the numbers visible on the balls:

| Image | Ball(s) | Cells | Problem | Correction |
|---|---|---|---|---|
| Table1 | 2 | D4, E4 | Row value 1212 is impossible (table has 683 rows) | Swapped Row and Column: (429, 1212) |
| Table1 | 4 | H4, I4 | Row value 1050 is impossible | Swapped: (335, 1050) |
| Table1 | 11 | V4, W4 | Row value 1169 is impossible | Swapped: (407, 1169) |
| Table1 | 3 | F4, G4 | Same position as ball 7 | Estimated from the rack spacing: (381, 1132) |
| Table2 | 4 and 5 | H5-K5 | Positions of the two balls swapped | 4 = (315, 1046), 5 = (310, 1305) |
| AllBalls | 11-15 | V3-AE3 | Labels in the wrong order | 11 = (335, 1013), 12 = (336, 1067), 13 = (334, 1130), 14 = (345, 1187), 15 = (342, 1245) |

Effect on the results (6 test images):
| | Spreadsheet as released | Corrected |
|---|---|---|
| Stage 1 F1 | 0.910 | 0.955 |
| Recognition accuracy | 63.0% | 95.3% |
| Located and recognised | 56.7% | 90.0% |

The large recognition difference comes from the AllBalls labels: with the original
labels, the system learns the wrong colours for balls 11-15.

