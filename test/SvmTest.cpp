#include <string>
#include <fstream>
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "svm/Svm2Svg.h"

TEST(SvmTest, draft) {
    std::string input;
    input = "/home/andreas/Desktop/odr/svm/test.svm";
    //input = "/home/andreas/Desktop/odr/svm/Object 44";
    //input = "/home/andreas/Desktop/odr/svm/Object 45";
    const std::string output = "../../test/test.svg";

    std::ifstream in;
    in.open(input);

    std::ofstream out;
    out.open(output);

    odr::Svm2Svg::translate(in, out);

    LOG(INFO) << "done";
}