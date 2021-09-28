
#include <iostream>
#include <string>
#include <vector>
using namespace std;

class ParserPoint {
    public:
        ParserPoint(string name, int line) {
            beginLine = line;
            pointName = name;
        }
        string getPointName() {
            return pointName;
        }
        //void setContentArray(char *contentArray);
        vector<string> getContentArray() {
            return contentArray;
        }
        int count() {return arrayCount;}
        void addPoint(string name) {
            contentArray.push_back(name);
            arrayCount++;
            endLine = beginLine + arrayCount;
        }

        bool contains(string name) {
            for (int i = 0; i < arrayCount; i++) {
                if (name == contentArray[i]) return true;
            }
            return false;
        }

        void printContent() {
            cout << pointName << "@";
            cout << "[";
            for (int i = 0; i < arrayCount; i++) {
                cout << contentArray[i];
                if (arrayCount != i + 1) cout << ", ";
            }
            cout << "]" << endl;
        }

    private:
        int arrayCount = 0;
        int beginLine;
        int endLine = beginLine;
        string pointName;
        vector<string> contentArray;

};
