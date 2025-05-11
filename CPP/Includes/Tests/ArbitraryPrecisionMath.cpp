#include <iostream>
#include <string>
#include <algorithm>
#include <stdexcept>

class BigDecimal {
private:
    std::string value;
    bool isNegative;

    // Helper function to remove leading zeros
    std::string removeLeadingZeros(const std::string& str) const {
        size_t start = str.find_first_not_of('0');
        if (start == std::string::npos || str[start] == '.') {
            return "0";
        }
        return str.substr(start);
    }

    // Helper function to remove trailing zeros
    std::string removeTrailingZeros(const std::string& str) const {
        if (str.find('.') == std::string::npos) {
            return str;
        }
        size_t end = str.find_last_not_of('0');
        if (end == str.find('.')) {
            return str.substr(0, end);
        }
        return str.substr(0, end + 1);
    }

    // Helper function to add decimal points if missing
    void addDecimalPoints(std::string& num1, std::string& num2) const {
        int point1 = num1.find('.');
        int point2 = num2.find('.');

        if (point1 == std::string::npos) {
            num1 += ".0";
            point1 = num1.find('.');
        }
        if (point2 == std::string::npos) {
            num2 += ".0";
            point2 = num2.find('.');
        }

        // Split the numbers into integer and fractional parts
        std::string frac1 = num1.substr(point1 + 1);
        std::string frac2 = num2.substr(point2 + 1);

        // Pad the fractional parts with zeros
        while (frac1.size() < frac2.size()) frac1 += '0';
        while (frac2.size() < frac1.size()) frac2 += '0';

        num1 = num1.substr(0, point1) + "." + frac1;
        num2 = num2.substr(0, point2) + "." + frac2;
    }

    // Helper function to compare two strings as numbers
    int compareStrings(const std::string& num1, const std::string& num2) const {
        std::string num1_clean = removeLeadingZeros(num1);
        std::string num2_clean = removeLeadingZeros(num2);

        if (num1_clean.size() > num2_clean.size()) return 1;
        if (num1_clean.size() < num2_clean.size()) return -1;

        for (size_t i = 0; i < num1_clean.size(); ++i) {
            if (num1_clean[i] > num2_clean[i]) return 1;
            if (num1_clean[i] < num2_clean[i]) return -1;
        }
        return 0;
    }

    // Helper function to subtract two BigDecimal numbers (used in divide)
    std::string subtractStrings(const std::string& num1, const std::string& num2) const {
        std::string result = "";
        int borrow = 0;
        int diffSize = num1.size() - num2.size();

        for (int i = num2.size() - 1; i >= 0; --i) {
            int diff = (num1[i + diffSize] - '0') - (num2[i] - '0') - borrow;
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result.insert(result.begin(), diff + '0');
        }

        for (int i = diffSize - 1; i >= 0; --i) {
            int diff = (num1[i] - '0') - borrow;
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result.insert(result.begin(), diff + '0');
        }

        return removeLeadingZeros(result);
    }

public:
    BigDecimal(const std::string& value) {
        if (value[0] == '-') {
            isNegative = true;
            this->value = value.substr(1);
        } else {
            isNegative = false;
            this->value = value;
        }
    }

    // Function to add two BigDecimal numbers
    BigDecimal add(const BigDecimal& other) const {
        if (isNegative == other.isNegative) {
            std::string num1 = value;
            std::string num2 = other.value;

            addDecimalPoints(num1, num2);

            // Split the numbers into integer and fractional parts
            int point1 = num1.find('.');
            int point2 = num2.find('.');

            std::string int1 = num1.substr(0, point1);
            std::string frac1 = num1.substr(point1 + 1);
            std::string int2 = num2.substr(0, point2);
            std::string frac2 = num2.substr(point2 + 1);

            // Pad the integer parts with zeros
            while (int1.size() < int2.size()) int1.insert(int1.begin(), '0');
            while (int2.size() < int1.size()) int2.insert(int1.begin(), '0');

            // Add the integer and fractional parts
            std::string intResult = "";
            std::string fracResult = "";
            int carry = 0;

            // Add fractional parts
            for (int i = frac1.size() - 1; i >= 0; --i) {
                int sum = (frac1[i] - '0') + (frac2[i] - '0') + carry;
                fracResult.insert(fracResult.begin(), (sum % 10) + '0');
                carry = sum / 10;
            }

            // Add integer parts
            for (int i = int1.size() - 1; i >= 0; --i) {
                int sum = (int1[i] - '0') + (int2[i] - '0') + carry;
                intResult.insert(intResult.begin(), (sum % 10) + '0');
                carry = sum / 10;
            }

            if (carry > 0) {
                intResult.insert(intResult.begin(), carry + '0');
            }

            // Remove leading and trailing zeros
            intResult = removeLeadingZeros(intResult);
            fracResult = removeTrailingZeros(fracResult);

            if (fracResult.empty()) {
                return BigDecimal((isNegative ? "-" : "") + intResult);
            } else {
                return BigDecimal((isNegative ? "-" : "") + intResult + "." + fracResult);
            }
        } else {
            if (isNegative) {
                return other.subtract(BigDecimal(value));
            } else {
                return subtract(BigDecimal(other.value));
            }
        }
    }

    // Function to subtract two BigDecimal numbers
    BigDecimal subtract(const BigDecimal& other) const {
        if (isNegative != other.isNegative) {
            return add(BigDecimal((other.isNegative ? "" : "-") + other.value));
        }

        std::string num1 = value;
        std::string num2 = other.value;

        addDecimalPoints(num1, num2);

        bool resultIsNegative = false;
        if (compareStrings(num1, num2) < 0) {
            resultIsNegative = true;
            std::swap(num1, num2);
        }

        // Split the numbers into integer and fractional parts
        int point1 = num1.find('.');
        int point2 = num2.find('.');

        std::string int1 = num1.substr(0, point1);
        std::string frac1 = num1.substr(point1 + 1);
        std::string int2 = num2.substr(0, point2);
        std::string frac2 = num2.substr(point2 + 1);

        // Pad the integer parts with zeros
        while (int1.size() < int2.size()) int1.insert(int1.begin(), '0');
        while (int2.size() < int1.size()) int2.insert(int1.begin(), '0');

        // Subtract the integer and fractional parts
        std::string intResult = "";
        std::string fracResult = "";
        int borrow = 0;

        // Subtract fractional parts
        for (int i = frac1.size() - 1; i >= 0; --i) {
            int diff = (frac1[i] - '0') - (frac2[i] - '0') - borrow;
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            fracResult.insert(fracResult.begin(), diff + '0');
        }

        // Subtract integer parts
        for (int i = int1.size() - 1; i >= 0; --i) {
            int diff = (int1[i] - '0') - (int2[i] - '0') - borrow;
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            intResult.insert(intResult.begin(), diff + '0');
        }

        // Remove leading and trailing zeros
        intResult = removeLeadingZeros(intResult);
        fracResult = removeTrailingZeros(fracResult);

        if (fracResult.empty()) {
            return BigDecimal((resultIsNegative ? "-" : "") + intResult);
        } else {
            return BigDecimal((resultIsNegative ? "-" : "") + intResult + "." + fracResult);
        }
    }

    // Function to multiply two BigDecimal numbers
    BigDecimal multiply(const BigDecimal& other) const {
        std::string num1 = value;
        std::string num2 = other.value;

        // Remove decimal points and calculate the number of decimal places
        int point1 = num1.find('.');
        int point2 = num2.find('.');

        if (point1 == std::string::npos) point1 = num1.size();
        if (point2 == std::string::npos) point2 = num2.size();

        num1.erase(point1, 1);
        num2.erase(point2, 1);

        int decimalPlaces = (num1.size() - point1) + (num2.size() - point2);

        // Perform multiplication
        std::string result(num1.size() + num2.size(), '0');

        for (int i = num1.size() - 1; i >= 0; --i) {
            int carry = 0;
            for (int j = num2.size() - 1; j >= 0; --j) {
                int sum = (num1[i] - '0') * (num2[j] - '0') + (result[i + j + 1] - '0') + carry;
                carry = sum / 10;
                result[i + j + 1] = (sum % 10) + '0';
            }
            result[i + num2.size()] += carry;
        }

        // Insert the decimal point
        result.insert(result.size() - decimalPlaces, 1, '.');

        // Remove leading and trailing zeros
        result = removeLeadingZeros(result);
        result = removeTrailingZeros(result);

        bool resultIsNegative = (isNegative != other.isNegative);
        return BigDecimal((resultIsNegative ? "-" : "") + result);
    }

    // Function to divide two BigDecimal numbers with given precision
    BigDecimal divide(const BigDecimal& other, int precision) const {
        if (other.value == "0") {
            throw std::invalid_argument("Division by zero");
        }

        std::string num1 = value;
        std::string num2 = other.value;

        // Find the position of the decimal point
        int point1 = num1.find('.');
        int point2 = num2.find('.');

        // If there's no decimal point, add one at the end
        if (point1 == std::string::npos) {
            num1 += ".0";
            point1 = num1.find('.');
        }
        if (point2 == std::string::npos) {
            num2 += ".0";
            point2 = num2.find('.');
        }

        // Remove the decimal point for division
        num1.erase(point1, 1);
        num2.erase(point2, 1);

        int decimalPlaces = num1.size() - point1 + precision;
        num1.append(decimalPlaces, '0');

        std::string result;
        std::string dividend(num1.substr(0, num2.size()));
        std::string divisor(num2);

        int idx = num2.size();
        while (idx <= num1.size()) {
            int quotientDigit = 0;
            while (compareStrings(dividend, divisor) >= 0) {
                dividend = subtractStrings(dividend, divisor);
                ++quotientDigit;
            }

            result += std::to_string(quotientDigit);

            if (idx < num1.size()) {
                dividend += num1[idx];
            }
            dividend = removeLeadingZeros(dividend);
            ++idx;
        }

        // Insert the decimal point
        if (result.size() > precision) {
            result.insert(result.size() - precision, 1, '.');
        } else {
            result.insert(result.begin(), precision - result.size() + 1, '0');
            result[0] = '0';
            result.insert(1, ".");
        }

        // Remove trailing zeros
        result = removeTrailingZeros(result);

        bool resultIsNegative = (isNegative != other.isNegative);
        return BigDecimal((resultIsNegative ? "-" : "") + result);
    }

    friend std::ostream& operator<<(std::ostream& os, const BigDecimal& bd) {
        os << (bd.isNegative ? "-" : "") << bd.value;
        return os;
    }
};

int main() {
    BigDecimal num1("0.1");
    BigDecimal num2("0.2");

    BigDecimal resultAdd = num1.add(num2);
    std::cout << "0.1 + 0.2 = " << resultAdd << std::endl;

    BigDecimal resultSub = num1.subtract(num2);
    std::cout << "0.1 - 0.2 = " << resultSub << std::endl;

    BigDecimal num3("1");
    BigDecimal num4("3");

    BigDecimal resultDiv = num3.divide(num4, 10);
    std::cout << "1 / 3 with precision 10 = " << resultDiv << std::endl;

    BigDecimal resultMul = num3.multiply(num4);
    std::cout << "1 * 3 = " << resultMul << std::endl;

    BigDecimal num5("-0.1");
    BigDecimal resultNegAdd = num1.add(num5);
    std::cout << "0.1 + (-0.1) = " << resultNegAdd << std::endl;

    BigDecimal resultNegSub = num1.subtract(num5);
    std::cout << "0.1 - (-0.1) = " << resultNegSub << std::endl;

    return 0;
}
