#include <gtest/gtest.h>

#include "MMapFile.hpp"
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

namespace itch {
namespace {

// Temporary file helper
struct TempFile {
    std::string path;

    explicit TempFile(const std::string& content = "") {
        char tmpl[] = "/tmp/itch_gtest_XXXXXX";
        int fd = ::mkstemp(tmpl);
        EXPECT_GE(fd, 0);
        path = tmpl;
        if (!content.empty()) {
            EXPECT_EQ(::write(fd, content.data(), content.size()),
                      (ssize_t)content.size());
        }
        ::close(fd);
    }
    ~TempFile() { ::unlink(path.c_str()); }
};

TEST(MMapFile, MissingPathIsInvalid) {
    MMapFile m("/tmp/itch_definitely_does_not_exist_12345.bin");
    EXPECT_FALSE(m.valid());
    EXPECT_EQ(m.data(), nullptr);
    EXPECT_EQ(m.size(), 0u);
}

TEST(MMapFile, EmptyFileIsInvalid) {
    TempFile t("");  // 0 bytes
    MMapFile m(t.path.c_str());
    EXPECT_FALSE(m.valid());
}

TEST(MMapFile, DirectoryIsInvalid) {
    MMapFile m("/tmp");
    EXPECT_FALSE(m.valid());
}

TEST(MMapFile, RoundTripContentMatches) {
    std::string payload(4096, 'A');
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>('A' + (i % 26));
    TempFile t(payload);
    MMapFile m(t.path.c_str());
    ASSERT_TRUE(m.valid());
    EXPECT_EQ(m.size(), payload.size());
    EXPECT_EQ(std::memcmp(m.data(), payload.data(), payload.size()), 0);
    EXPECT_EQ(m.span().size(), payload.size());
    EXPECT_EQ(m.span()[0], static_cast<std::byte>('A'));
}

TEST(MMapFile, MoveTransfersOwnership) {
    TempFile t("hello-world");
    MMapFile a(t.path.c_str());
    ASSERT_TRUE(a.valid());
    const std::byte* p = a.data();
    std::size_t n = a.size();
    MMapFile b(std::move(a));
    EXPECT_EQ(b.data(), p);
    EXPECT_EQ(b.size(), n);
    EXPECT_TRUE(b.valid());
    EXPECT_EQ(a.data(), nullptr);  // source nulled
    EXPECT_FALSE(a.valid());
}

TEST(MMapFile, MoveAssignReleasesPrevious) {
    TempFile t1("first-file-payload");
    TempFile t2("second-file-payload!!");
    MMapFile a(t1.path.c_str());
    MMapFile b(t2.path.c_str());
    ASSERT_TRUE(a.valid());
    ASSERT_TRUE(b.valid());
    const std::byte* pa = a.data();  // a's mapping - b must adopt it
    const std::size_t na = a.size();
    b = std::move(a);
    EXPECT_EQ(b.data(), pa);  // b took over a's resources
    EXPECT_EQ(b.size(), na);
    EXPECT_TRUE(b.valid());
    EXPECT_EQ(a.data(), nullptr);  // source nulled
}

TEST(MMapFile, AdviseSequentialOnInvalidIsNoOp) {
    MMapFile m;  // default-constructed, null
    EXPECT_NO_FATAL_FAILURE(m.advise_sequential());
    EXPECT_EQ(m.prefault(), 0u);
}

TEST(MMapFile, AdviseSequentialDoesNotCrash) {
    TempFile t(std::string(8192, 'x'));
    MMapFile m(t.path.c_str());
    ASSERT_TRUE(m.valid());
    EXPECT_NO_FATAL_FAILURE(m.advise_sequential());
}

TEST(MMapFile, PrefaultTouchesEveryPage) {
    std::string payload(3 * 4096, 'z');
    TempFile t(payload);
    MMapFile m(t.path.c_str());
    ASSERT_TRUE(m.valid());
    EXPECT_EQ(m.prefault(), 3 * 4096u);
}

} // namespace
} // namespace itch