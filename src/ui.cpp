#include "ui.h"
#include "serial.h"
#include "recording.h"
#include <filesystem>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include "config.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cstring>

void initializeUI(int windowWidth, int windowHeight) {
    // Top bar height (for window controls)
    int topBarHeight = 40;

    // Set navigation bar height to 80px as required
    int bottomBarHeight = 80;

    // Window control buttons (top-right)
    closeButtonRect = Rect(windowWidth - 35, 5, 30, 30);
    minimizeButtonRect = Rect(windowWidth - 70, 5, 30, 30);

    // Bottom navigation bar (full width, 80px height at bottom)
    navBarRect = Rect(0, windowHeight - bottomBarHeight, windowWidth, bottomBarHeight);

    // Main video area (centered, max height 720px)
    int videoHeight = min(720, windowHeight - topBarHeight - bottomBarHeight);
    int yOffset = topBarHeight;  // position video right below top bar
    videoRect = Rect(0, yOffset, windowWidth, videoHeight);

    // Record and export buttons (top)
    recordButtonRect = Rect(windowWidth - 320, 5, 150, 30);
    exportButtonRect = Rect(windowWidth - 160, 5, 150, 30);

    // Bottom bar buttons
    int btnWidth = 120;
    int btnHeight = 30;
    int startX = 10;

    // Position buttons in the navigation bar
    int buttonY = windowHeight - bottomBarHeight + (bottomBarHeight - btnHeight) / 2;

    zoomInButtonRect = Rect(startX, buttonY, btnWidth, btnHeight);
    startX += btnWidth + 10;

    zoomOutButtonRect = Rect(startX, buttonY, btnWidth, btnHeight);

    // Status area
    logLabelRect = Rect(10, 5, windowWidth - 330, 30);

    // Update global constant for consistent usage
    NAV_BAR_HEIGHT = bottomBarHeight;

    // Initialize navigation bar
    initNavigationBar(windowWidth, windowHeight);

    // Initialize IR controls in a suitable position
    // Since background subtraction is removed, we can position the ICR controls directly
    int controlsX = 10;
    int controlsY = topBarHeight + 10;
    int controlsWidth = 400;
    int controlsHeight = 100;
    
    initIR(controlsX, controlsY, controlsWidth, controlsHeight);
}

void mouseCallback(int event, int x, int y, int flags, void* userdata) {
    appConfig.loadConfig();
    if (event == EVENT_LBUTTONDOWN) {
        if (toggleNavButtonRect.contains(Point(x, y))) {
            showNavBar = !showNavBar;
            return;
        }
        
        // Check for ICR Mode button click
        if (icrButtonRect.contains(Point(x, y))) {
            icrModeEnabled = !icrModeEnabled;
            sendICRCommand(icrModeEnabled);
            return;
        }
        
        // Check for Stabilizer button click
        if (stabilizerButtonRect.contains(Point(x, y))) {
            stabilizerEnabled = !stabilizerEnabled;
            sendStabilizerCommand(stabilizerEnabled);
            return;
        }

        if (showExportDialog) {
            // File checkboxes handling
            bool fileAreaClicked = false;

            // Check if click was on any file checkbox
            for (size_t i = 0; i < mouseData.fileCheckboxRects.size(); i++) {
                if (mouseData.fileCheckboxRects[i].contains(Point(x, y))) {
                    size_t fileIndex = i + scrollOffset;
                    if (fileIndex < fileSelection.size()) {
                        fileSelection[fileIndex] = !fileSelection[fileIndex];
                        fileAreaClicked = true;
                        break;
                    }
                }
            }

            // Check if click was on any file text
            if (!fileAreaClicked) {
                for (size_t i = 0; i < mouseData.fileTextRects.size(); i++) {
                    if (mouseData.fileTextRects[i].contains(Point(x, y))) {
                        size_t fileIndex = i + scrollOffset;
                        if (fileIndex < fileSelection.size()) {
                            fileSelection[fileIndex] = !fileSelection[fileIndex];
                            fileAreaClicked = true;
                            break;
                        }
                    }
                }
            }

            // Check if browse button was clicked
            if (!fileAreaClicked && dirSelectRect.contains(Point(x, y))) {
                string selectedDir = openDirectoryBrowser();
                if (!selectedDir.empty()) {
                    exportDestDir = selectedDir;
                }
            }
            // Check if click was on "Keep original files" checkbox or its text
            else if (!fileAreaClicked &&
                    (Rect(keepFilesRect.x, keepFilesRect.y, 20, 20).contains(Point(x, y)) ||
                     Rect(keepFilesRect.x + 25, keepFilesRect.y, 150, 20).contains(Point(x, y)))) {
                keepOriginalFiles = !keepOriginalFiles;
            }
            // Check if click was on scroll buttons
            else if (!fileAreaClicked && scrollUpRect.contains(Point(x, y)) && scrollOffset > 0) {
                scrollOffset--;
            }
            else if (!fileAreaClicked && scrollDownRect.contains(Point(x, y)) &&
                     scrollOffset < recordingFiles.size() - maxFilesVisible) {
                scrollOffset++;
            }
            // Check if Export button was clicked
            else if (!fileAreaClicked && exportConfirmRect.contains(Point(x, y))) {
                performExport();
                showExportDialog = false;
            }
            // Check if Cancel button was clicked
            else if (!fileAreaClicked && (exportCancelRect.contains(Point(x, y)) ||
                     !exportDialogRect.contains(Point(x, y)))) {
                showExportDialog = false;
                setLogMessage("");
            }
            
            if (!showExportDialog) 
                return;
        }
        
        if (minimizeButtonRect.contains(Point(x, y))) {
            // Just minimize the window
            system("xdotool search --name \"Water Dripping Investigation Recording Tools\" windowminimize");
            return;
        }
        else if (closeButtonRect.contains(Point(x, y))) {
            // Close window
            destroyWindow("Water Dripping Investigation Recording Tools");
            return;
        }

        // If nav bar is not showing, only handle toggle button
        if (!showNavBar) {
            return;
        }

        if (recordButtonRect.contains(Point(x, y))) {
            // Record button clicked
            if (isProcessing) {
                // Don't allow starting a new recording while processing
                setLogMessage("Processing...");
                return;
            }
            
            isRecording = !isRecording;

            if (isRecording) {
                // Start recording code
                time_t now = time(0);
                char buffer[80];
                strftime(buffer, 80, "%Y%m%d_%H%M%S", localtime(&now));
                tempFilename = "/tmp/" + string(buffer) + "_temp.avi";

                recordingStartTime = system_clock::now();
                setLogMessage("Rec started...");
                progressValue = 0;
            } else {
                // Stop recording code
                if (videoWriter.isOpened()) {
                    recordingDurationSeconds = duration<double>(system_clock::now() - recordingStartTime).count();
                    videoWriter.release();
                    setLogMessage("Rec stopped");

                    // Cancel any ongoing processing
                    if (isProcessing && processingThread.joinable()) {
                        isProcessing = false;
                        processingThread.join();
                    }
                    
                    // Start processing in a new thread using the temp file
                    isProcessing = true;
                    if (processingThread.joinable()) {
                        processingThread.join();
                    }
                    processingThread = thread(postProcessVideo, tempFilename, recordingDurationSeconds);
                }
            }
        } else if (exportButtonRect.contains(Point(x, y))) {
            // Export button clicked - show export dialog
            if (!isRecording) {
                showExportDialog = true;
                createExportDialog(DISPLAY_WIDTH, DISPLAY_HEIGHT);
                scanRecordingDirectory();
                setLogMessage("Preparing export dialog...");
            } else {
                setLogMessage("Stop rec before exporting");
            }
        } else if (zoomInButtonRect.contains(Point(x, y))) {
            // Zoom in button pressed down
            isZoomInHeld = true;
            lastZoomTime = system_clock::now();
            // Perform initial zoom immediately
            zoomIn();
        } else if (zoomOutButtonRect.contains(Point(x, y))) {
            // Zoom out button pressed down
            isZoomOutHeld = true;
            lastZoomTime = system_clock::now();
            // Perform initial zoom immediately
            zoomOut();
        }
    }
    else if (event == EVENT_LBUTTONUP) {
        // Handle button releases
        isZoomInHeld = false;
        isZoomOutHeld = false;
    }
    else if (event == EVENT_MOUSEMOVE) {
        // If mouse moved outside the button area while button is held, stop zooming
        if (isZoomInHeld && !zoomInButtonRect.contains(Point(x, y))) {
            isZoomInHeld = false;
        }
        if (isZoomOutHeld && !zoomOutButtonRect.contains(Point(x, y))) {
            isZoomOutHeld = false;
        }
    }
}

// Modify the initIR function to position controls correctly
void initIR(int x, int y, int width, int height) {
    // Position buttons in the panel
    int buttonY = y + 15;  
    int buttonHeight = 30;
    int buttonSpacing = 40;
    
    // Divide the width for two buttons with some spacing
    int buttonWidth = (width - 30) / 2;
    
    // Position ICR button on the left
    icrButtonRect = Rect(x + 10, buttonY, buttonWidth, buttonHeight);
    
    // Position Stabilizer button on the right
    stabilizerButtonRect = Rect(x + 10 + buttonWidth + 10, buttonY, buttonWidth, buttonHeight);
}

// Modify the drawIR function to draw ICR and Stabilizer controls
void drawIR(Mat& frame, bool bgActive) {
    // Don't draw controls if export dialog is showing
    if (showExportDialog) {
        return;
    }
    
    // Calculate control panel bounds to encompass both buttons
    int panelX = min(icrButtonRect.x, stabilizerButtonRect.x) - 10;
    int panelWidth = max(icrButtonRect.x + icrButtonRect.width, 
                         stabilizerButtonRect.x + stabilizerButtonRect.width) - panelX + 10;
    int panelY = icrButtonRect.y - 15;
    int panelHeight = icrButtonRect.height + 30;
    
    // Draw background panel for controls
    Rect controlsRect(panelX, panelY, panelWidth, panelHeight);
                     
    rectangle(frame, controlsRect, Scalar(30, 40, 30, 180), -1);  
    rectangle(frame, controlsRect, Scalar(100, 150, 100), 2);
    
    // Draw ICR Mode button
    Scalar icrButtonColor = icrModeEnabled ? BUTTON_COLOR : Scalar(100, 100, 100);
    rectangle(frame, icrButtonRect, icrButtonColor, -1);
    rectangle(frame, icrButtonRect, HIGHLIGHT_COLOR, 1);
    putText(frame, "ICR Mode: " + string(icrModeEnabled ? "ON" : "OFF"), 
            Point(icrButtonRect.x + 10, icrButtonRect.y + 20),
            FONT_HERSHEY_SIMPLEX, 0.5, TEXT_COLOR, 1);
    
    // Draw Stabilizer button
    Scalar stabilizerButtonColor = stabilizerEnabled ? BUTTON_COLOR : Scalar(100, 100, 100);
    rectangle(frame, stabilizerButtonRect, stabilizerButtonColor, -1);
    rectangle(frame, stabilizerButtonRect, HIGHLIGHT_COLOR, 1);
    putText(frame, "Stabilizer: " + string(stabilizerEnabled ? "ON" : "OFF"), 
            Point(stabilizerButtonRect.x + 10, stabilizerButtonRect.y + 20),
            FONT_HERSHEY_SIMPLEX, 0.5, TEXT_COLOR, 1);
}

