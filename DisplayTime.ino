void displayTime(String timeStr) {

  int textSize = 7;
  int charWidth = 6 * textSize; // Assuming each character is 6 pixels wide and text size is 7
  int charHeight = 8 * textSize; // Assuming each character is 8 pixels high and text size is 7
  int len = timeStr.length();
  int totalWidth = len * charWidth;

  // Calculate the x position to center the string on the screen
  int xPos = (tft.width() - totalWidth) / 2;

  // Calculate the y position to center the string on the screen
  int yPos = (tft.height() - charHeight) / 2;

  // Determine previous string total width for clearing purposes
  int prevLen = previousTimeStr.length();
  int prevTotalWidth = prevLen * charWidth;
  int prevXPos = (tft.width() - prevTotalWidth) / 2;

  // Compare each character of the new time string with the previous time string
  for (int i = 0; i < min(len, prevLen); i++) {
    if (timeStr[i] != previousTimeStr[i]) {
      // If characters are different, clear the old character
      tft.fillRect(prevXPos + i * charWidth, yPos, charWidth, charHeight, TFT_BLACK);
    }
  }

  // Clear remaining characters if the new string is shorter than the previous string
  if (len < prevLen) {
    for (int i = len; i < prevLen; i++) {
      tft.fillRect(prevXPos + i * charWidth, yPos, charWidth, charHeight, TFT_BLACK);
    }
  }

  // Clear the area if the new string is longer than the previous string
  if (len > prevLen) {
    for (int i = prevLen; i < len; i++) {
      tft.fillRect(prevXPos + i * charWidth, yPos, charWidth, charHeight, TFT_BLACK);
    }
  }

  // Set text size and color for time
  tft.setTextSize(textSize);
  tft.setCursor(xPos, yPos); // Set the cursor to the calculated x and y positions to center the string
  tft.print(timeStr);

  // Update the previous time string
  previousTimeStr = timeStr;
}
