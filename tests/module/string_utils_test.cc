/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2016 Couchbase, Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "utils/string_utils.h"

using phosphor::utils::format_string;

TEST(FormatStringTest, empty) {
    EXPECT_EQ(format_string(""), "");
}

TEST(FormatStringTest, basic) {
    EXPECT_EQ(format_string("hello, world"), "hello, world");
}

TEST(FormatStringTest, substitution) {
    EXPECT_EQ(format_string("%s", "hello, world"), "hello, world");
}

TEST(FormatStringTest, numbers) {
    EXPECT_EQ(format_string("%d", 33), "33");
}

using phosphor::utils::to_json;

TEST(ToJSONTest, test) {
    // TODO: More extensive testing of escaping
    EXPECT_EQ("\"Hello, World\"", to_json("Hello, World"));
    EXPECT_EQ("\"\\b\\f\\n\\r\\t\\\\\\/\\\"\"", to_json("\b\f\n\r\t\\/\""));
}

using phosphor::utils::split_string;

TEST(SplitStringTest, test) {
    EXPECT_THAT(split_string("ab"), testing::ElementsAre("ab"));

    EXPECT_THAT(split_string("Hello, World"),
                testing::ElementsAre("Hello,", "World"));

    EXPECT_THAT(split_string("Hello, World", ','),
                testing::ElementsAre("Hello", " World"));

    EXPECT_THAT(split_string(""), testing::ElementsAre(""));

    EXPECT_THAT(split_string(",,,", ','), testing::ElementsAre("", "", ""));
}

using phosphor::utils::string_replace;

TEST(SplitReplaceTest, test) {
    std::string target("Hello, World!");
    EXPECT_EQ("Hello, Will!", string_replace(target, "World", "Will"));
    EXPECT_EQ("Hello, Will!", string_replace(target, "Phosphor", "BruceWayne"));
    EXPECT_EQ("Hello, Will!", string_replace(target, "", "Oswald"));
}

using phosphor::utils::glob_match;

TEST(GlobTest, test) {
    EXPECT_TRUE(glob_match("*", "helloworld.json"));
    EXPECT_TRUE(glob_match("*.json", "helloworld.json"));
    EXPECT_TRUE(glob_match("hello*.json", "helloworld.json"));
    EXPECT_TRUE(glob_match("helloworld?json", "helloworld.json"));
    EXPECT_TRUE(glob_match("*h", "h"));
    EXPECT_TRUE(glob_match("*", "h"));
    EXPECT_TRUE(glob_match("*", ""));
    EXPECT_TRUE(glob_match("heya*", "heya"));
    EXPECT_TRUE(glob_match("helloworld", "helloworld"));
    EXPECT_TRUE(glob_match("hello*world", std::string("hello\0\0\0world", 13)));

    EXPECT_FALSE(glob_match("hello", "world"));
    EXPECT_FALSE(glob_match("a", ""));
    EXPECT_FALSE(glob_match("?", ""));
    EXPECT_FALSE(glob_match("*a", "b"));

    EXPECT_TRUE(glob_match("+", "helloworld.json"));
    EXPECT_TRUE(glob_match("+.json", "helloworld.json"));
    EXPECT_TRUE(glob_match("hello+.json", "helloworld.json"));
    EXPECT_FALSE(glob_match("+h", "h"));
    EXPECT_TRUE(glob_match("+", "h"));
    EXPECT_FALSE(glob_match("+", ""));
    EXPECT_FALSE(glob_match("heya+", "heya"));
    EXPECT_TRUE(glob_match("helloworld", "helloworld"));
    EXPECT_TRUE(glob_match("hello+world", std::string("hello\0\0\0world", 13)));
}