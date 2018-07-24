/* Requires */
var gulp = require('gulp');
var plumber = require('gulp-plumber');
var concat = require('gulp-concat');
var htmlmin = require('gulp-htmlmin');
var cleancss = require('gulp-clean-css');
var uglifyjs = require('gulp-uglify');
var gzip = require('gulp-gzip');
var del = require('del');
var markdown = require('gulp-markdown-github-style');
var rename = require('gulp-rename');

/* HTML Task */
gulp.task('html', function() {
    return gulp.src(['html/*.html', 'html/*.htm'])
        .pipe(plumber())
        .pipe(htmlmin({
            collapseWhitespace: true,
            removeComments: true,
            minifyCSS: true,
            minifyJS: true}))
        .pipe(gzip())
        .pipe(gulp.dest('data/www'));
});

/* CSS Task */
gulp.task('css', function() {
    return gulp.src(['html/css/bootstrap.css', 'html/style.css'])
        .pipe(plumber())
        .pipe(concat('esps.css'))
        .pipe(cleancss())
        .pipe(gzip())
        .pipe(gulp.dest('data/www'));
});

/* JavaScript Task */
gulp.task('js', function() {
    return gulp.src(['html/js/jquery*.js', 'html/js/bootstrap.js', 'html/js/jqColorPicker.js', 'html/script.js'])
        .pipe(plumber())
        .pipe(concat('esps.js'))
        .pipe(uglifyjs())
        .pipe(gzip())
        .pipe(gulp.dest('data/www'));
});

/* Image Task */
gulp.task('image', function() {
    return gulp.src(['html/**/*.png', 'html/**/*.ico'])
        .pipe(plumber())
        .pipe(gulp.dest('data/www'));
});


/* Clean Task */
gulp.task('clean', function() {
    return del(['data/www/*']);
});

/* Markdown to HTML Task */
gulp.task('md', function() {
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
});

/* Travis specific stuff */
gulp.task('travis', function() {
    gulp.src(['travis/travis.md', 'dist/README.md'])
        .pipe(plumber())
        .pipe(concat('README.html'))
        .pipe(markdown())
        .pipe(gulp.dest('dist'));
});

/* Watch Task */
gulp.task('watch', function() {
    gulp.watch('html/*.html', ['html']);
    gulp.watch('html/*.htm', ['html']);
    gulp.watch('html/**/*.css', ['css']);
    gulp.watch('html/**/*.js', ['js']);
});

/* Default Task */
gulp.task('default', ['clean', 'html', 'css', 'js', 'image']);
