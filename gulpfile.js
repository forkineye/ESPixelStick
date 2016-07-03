/* Requires */
var gulp = require('gulp');
var htmlmin = require('gulp-htmlmin');
var cleancss = require('gulp-clean-css');
var uglifyjs = require('gulp-uglify');
var gzip = require('gulp-gzip');
var del = require('del');

/* HTML Task */
gulp.task('html', function() {
    return gulp.src(['html/*.html', 'html/*.htm'])
        .pipe(htmlmin({
            collapseWhitespace: true,
            removeComments: true}))
        .pipe(gzip())
        .pipe(gulp.dest('data/www'));
});

/* CSS Task */
gulp.task('css', function() {
    return gulp.src(['html/*.css'])
        .pipe(cleancss())
        .pipe(gzip())
        .pipe(gulp.dest('data/www'));
});

/* JavaScript Task */
gulp.task('js', function() {
    return gulp.src(['html/*.js'])
        .pipe(uglifyjs())
        .pipe(gzip())
        .pipe(gulp.dest('data/www'));
});

/* Clean Task */
gulp.task('clean', function() {
    return del(['data/www/*']);
});

/* Watch Task */
gulp.task('watch', function() {
    gulp.watch('html/*.html', ['html']);
    gulp.watch('html/*.htm', ['html']);
    gulp.watch('html/*.css', ['css']);
    gulp.watch('html/*.js', ['js']);
});

/* Default Task */
gulp.task('default', ['html', 'css', 'js']);
