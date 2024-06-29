
// Function to check if the given year is a leap year
bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// Function to calculate the number of days in the given month and year
int daysInMonth(int year, int month) {
  if (month == 2) { // February
    return isLeapYear(year) ? 29 : 28;
  }
  if (month == 4 || month == 6 || month == 9 || month == 11) {
    return 30;
  }
  return 31;
}

// Function to convert date-time to Unix time
unsigned long convertToUnixTime(const char* dateTime) {
  int year, month, day, hour, minute, second;
  sscanf(dateTime, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);

  // Calculate days since Unix epoch (1970-01-01)
  unsigned long days = 0;
  for (int y = 1970; y < year; y++) {
    days += isLeapYear(y) ? 366 : 365;
  }
  for (int m = 1; m < month; m++) {
    days += daysInMonth(year, m);
  }
  days += day - 1;

  // Convert days to seconds
  unsigned long seconds = days * 24 * 3600;
  // Add hours, minutes, and seconds
  seconds += hour * 3600;
  seconds += minute * 60;
  seconds += second;

  return seconds;
}

String formatTime(long seconds) {
  // Calculate hours, minutes, and seconds
  int hours = seconds / 3600;
  int minutes = (seconds % 3600) / 60;
  int secs = seconds % 60;

  // Create a string to hold the formatted time
  char formattedTime[9]; // "hh:mm:ss\0"

  // Format the time string
  snprintf(formattedTime, sizeof(formattedTime), "%02d:%02d:%02d", hours, minutes, secs);

  // Return the formatted time as a String object
  return String(formattedTime);
}