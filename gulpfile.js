/* Requires */
var gulp = require('gulp');
var plumber = require('gulp-plumber');
var concat = require('gulp-concat');
var htmlmin = require('gulp-htmlmin');
var cleancss = require('gulp-clean-css');
var terser = require('gulp-terser');
var gzip = require('gulp-gzip');
var del = require('del');
var markdown = require('gulp-markdown-github-style');
var rename = require('gulp-rename');

/* HTML Task */
gulp.task('html', function() {
    return gulp.src(['html/*.html'])
        .pipe(plumber())
        .pipe(htmlmin({
            collapseWhitespace: true,
            removeComments: true,
            minifyCSS: true,
            minifyJS: true}))
        .pipe(gzip())
        .pipe(gulp.dest('ESPixelStick/data/www'));
});

/* CSS Task */
gulp.task('css', function() {
    return gulp.src(['html/css/bootstrap.css', 'html/css/dropzone.css', 'html/css/style.css'])
        .pipe(plumber())
        .pipe(concat('esps.css'))
        .pipe(cleancss())
        .pipe(gzip())
        .pipe(gulp.dest('ESPixelStick/data/www'));
});

/* JavaScript Task */
gulp.task('js', function() {
    return gulp.src(['html/js/jquery*.js', 'html/js/bootstrap.js', 'html/js/jqColorPicker.js', 'html/script.js', 'html/js/dropzone.js', 'html/js/FileSaver.js', 'html/script.js'])
        .pipe(plumber())
        .pipe(concat('esps.js'))
        .pipe(terser({ 'toplevel': true }))
        .pipe(gzip())
        .pipe(gulp.dest('ESPixelStick/data/www'));
});

/* json Task */
gulp.task('json', function() {
    return gulp.src('html/*.json')
        .pipe(plumber())
        .pipe(gulp.dest('ESPixelStick/data'));
});

/* Image Task */
gulp.task('image', function() {
    return gulp.src(['html/**/*.png', 'html/**/*.ico'])
        .pipe(plumber())
        .pipe(gulp.dest('ESPixelStick/data/www'));
});

/* Clean Task */
gulp.task('clean', function() {
    return del(['ESPixelStick/data/www/*']);
});

/* Markdown to HTML Task */
gulp.task('md', function(done) {
    gulp.src('README.md')
        .pipe(plumber())
        .pipe(rename('ESPixelStick.html'))
        .pipe(markdown())
        .pipe(gulp.dest('dist'));
    gulp.src('Changelog.md')
        .pipe(plumber())
        .pipe(rename('Changelog.html'))
        .pipe(markdown())
        .pipe(gulp.dest('dist'));
    gulp.src('dist/README.md')
        .pipe(plumber())
        .pipe(rename('README.html'))
        .pipe(markdown())
        .pipe(gulp.dest('dist'));
    done();
});

/* CI specific stuff */
gulp.task('ci', function(done) {
    gulp.src(['.ci/warning.md', 'dist/README.md'])
        .pipe(plumber())
        .pipe(concat('README.html'))
        .pipe(markdown())
        .pipe(gulp.dest('dist'));
    done();
});

/* Watch Task */
gulp.task('watch', function() {
    gulp.watch('html/*.html', gulp.series('html'));
    gulp.watch('html/**/*.css', gulp.series('css'));
    gulp.watch('html/**/*.js', gulp.series('js'));
});

/* Default Task */
gulp.task('default', gulp.series(['clean', 'html', 'css', 'js', 'image', 'json']));
