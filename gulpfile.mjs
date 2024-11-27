/* Imports */
import gulp from "gulp";
const { series, parallel, src, dest, task } = gulp;
import plumber from "gulp-plumber";
import concat from "gulp-concat";
import htmlmin from "gulp-htmlmin";
import cleancss from "gulp-clean-css";
import terser from "gulp-terser";
import gzip from "gulp-gzip";
import { deleteAsync } from "del";
import markdown from "gulp-markdown-github-style";
import rename from "gulp-rename";
import using from "gulp-using";

/* HTML Task */
gulp.task("html", function () {
  return gulp
    .src(["html/*.html"])
    .pipe(using())
    .pipe(plumber())
    .pipe(
      htmlmin({
        collapseWhitespace: true,
        removeComments: true,
        minifyCSS: true,
        minifyJS: true,
      })
    )
    .pipe(gzip())
    .pipe(gulp.dest("data/www"));
});

/* CSS Task */
gulp.task("css", function () {
  return gulp
    .src(["html/css/*.css"])
    .pipe(plumber())
    .pipe(using())
    .pipe(concat("esps.css"))
    .pipe(cleancss())
    .pipe(gzip())
    .pipe(gulp.dest("data/www"));
});

/* JavaScript Task */
gulp.task("js", function () {
  return gulp
    .src([
      "html/js/jquery-*.js",
      "html/js/bootstrap.js",
      "html/js/jqColorPicker.js",
      "html/js/dropzone.js",
      "html/js/FileSaver.js",
      "html/js/jquery.cookie.js",
      "html/script.js",
    ])
    .pipe(plumber())
    .pipe(using())
    .pipe(concat("esps.js"))
    .pipe(
      terser({ toplevel: true })
    ) /* comment out this line to debug the script file */
    .pipe(gzip())
    .pipe(using())
    .pipe(gulp.dest("data/www"));
});

/* json Task */
gulp.task("json", function () {
  return gulp
    .src("html/*.json")
    .pipe(using())
    .pipe(plumber())
    .pipe(gulp.dest("data"));
});

/* Image Task */
gulp.task("image", function () {
  return gulp
    .src(["html/**/*.png", "html/**/*.ico"])
    .pipe(using())
    .pipe(plumber())
    .pipe(gulp.dest("data/www"));
});

/* Clean Task */
gulp.task("clean", function () {
  return deleteAsync(["data/www/*"]);
});

/* Markdown to HTML Task */
gulp.task("md", function (done) {
  gulp
    .src("README.md")
    .pipe(plumber())
    .pipe(rename("ESPixelStick.html"))
    .pipe(markdown())
    .pipe(gulp.dest("dist"));
  gulp
    .src("Changelog.md")
    .pipe(plumber())
    .pipe(rename("Changelog.html"))
    .pipe(markdown())
    .pipe(gulp.dest("dist"));
  gulp
    .src("dist/README.md")
    .pipe(plumber())
    .pipe(rename("README.html"))
    .pipe(markdown())
    .pipe(gulp.dest("dist"));
  done();
});

/* CI specific stuff */
gulp.task("ci", function (done) {
  gulp
    .src([".ci/warning.md", "dist/README.md"])
    .pipe(plumber())
    .pipe(concat("README.html"))
    .pipe(markdown())
    .pipe(gulp.dest("dist"));
  done();
});

/* Watch Task */
gulp.task("watch", function () {
  gulp.watch("html/*.html", gulp.series("html"));
  gulp.watch("html/**/*.css", gulp.series("css"));
  gulp.watch("html/**/*.js", gulp.series("js"));
});

/* Default Task */
gulp.task(
  "default",
  gulp.series(["clean", "html", "css", "js", "image", "json"])
);
