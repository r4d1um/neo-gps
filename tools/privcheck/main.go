// privcheck searches a repo's files for content that has no place in a
// public repo: private IP addresses, home network host names, email
// addresses and typical credentials.
//
//	go run ./tools/privcheck [directory]     (default: .)
//
// Exit code 1 if anything was found; every finding is printed as
// file:line: reason on stdout. The patterns are deliberately generic so
// this file itself doesn't need to contain anything private.
//
// Exception for one line: the comment "privcheck:ok" on the same line.
package main

import (
	"bufio"
	"bytes"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"strings"
)

type rule struct {
	why string
	re  *regexp.Regexp
}

var rules = []rule{
	{"private IPv4 (RFC 1918)", regexp.MustCompile(`\b(10\.\d{1,3}\.\d{1,3}\.\d{1,3}|192\.168\.\d{1,3}\.\d{1,3}|172\.(1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})\b`)},
	{"home network host name", regexp.MustCompile(`(?i)\b[a-z0-9-]+\.(fritz\.box|lan|home\.arpa|internal)\b`)},
	{"email address", regexp.MustCompile(`[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}`)},
	{"GitHub-Token", regexp.MustCompile(`\b(ghp|gho|ghu|ghs|ghr|github_pat)_[A-Za-z0-9_]{20,}`)},
	{"AWS-Key", regexp.MustCompile(`\bAKIA[0-9A-Z]{16}\b`)},
	{"private key", regexp.MustCompile(`-----BEGIN [A-Z ]*PRIVATE KEY-----`)},
	{"third-party web resource (sends visitors' IPs elsewhere, GDPR)", regexp.MustCompile(`(?i)(href|src)\s*=\s*["']https?://(fonts\.googleapis\.com|fonts\.gstatic\.com|cdn\.jsdelivr\.net|cdnjs\.cloudflare\.com|unpkg\.com|ajax\.googleapis\.com)`)},
	{"plain-text password", regexp.MustCompile(`(?i)\b(password|passwd|passwort|wifi_pass|wlan_pass)\s*[:=]\s*["'][^"'\s]{4,}["']`)},
}

// allowed email addresses: placeholders and GitHub noreply
var okMail = regexp.MustCompile(`(?i)@(example\.(com|org|net)|users\.noreply\.github\.com|noreply\.github\.com|anthropic\.com)$`)

func main() {
	root := "."
	if len(os.Args) > 1 {
		root = os.Args[1]
	}
	n, err := scan(root, os.Stdout)
	if err != nil {
		fmt.Fprintln(os.Stderr, "privcheck:", err)
		os.Exit(2)
	}
	if n > 0 {
		fmt.Printf("privcheck: %d finding(s)\n", n)
		os.Exit(1)
	}
	fmt.Println("privcheck: nothing found")
}

// scan checks all text files under root (except .git, build folders and
// binary files) and returns the number of findings.
func scan(root string, out *os.File) (int, error) {
	found := 0
	err := filepath.WalkDir(root, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			// our own test data contains hits on purpose
			if filepath.ToSlash(p) == "tools/privcheck" || strings.HasSuffix(filepath.ToSlash(p), "/tools/privcheck") {
				return filepath.SkipDir
			}
			switch d.Name() {
			case ".git", "build", "cbuild", "dist", "node_modules":
				return filepath.SkipDir
			}
			return nil
		}
		data, err := os.ReadFile(p)
		if err != nil {
			return err
		}
		if bytes.IndexByte(data, 0) >= 0 { // binary file
			return nil
		}
		sc := bufio.NewScanner(bytes.NewReader(data))
		sc.Buffer(make([]byte, 1<<20), 1<<20)
		for line := 1; sc.Scan(); line++ {
			for _, why := range check(sc.Text()) {
				fmt.Fprintf(out, "%s:%d: %s\n", p, line, why)
				found++
			}
		}
		return sc.Err()
	})
	return found, err
}

// check returns the reasons why a line stands out.
func check(s string) []string {
	if strings.Contains(s, "privcheck:ok") {
		return nil
	}
	var why []string
	for _, r := range rules {
		for _, m := range r.re.FindAllString(s, -1) {
			if r.why == "email address" && okMail.MatchString(m) {
				continue
			}
			why = append(why, r.why+": "+m)
			break
		}
	}
	return why
}
