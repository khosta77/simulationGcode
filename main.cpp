#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <limits> 
#include <istream>
#include <iomanip>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <cstdlib>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {  // jpeglib.h
#include <stdio.h>
#include <setjmp.h>
#include <unistd.h>
#include <stdlib.h>
#include <jconfig.h>
#include <jpeglib.h>
}

#define SIZE_STEPS          200  // количество шагов на оборот для двигателя (в наших примерах 200)
#define MICROSTEP           16  // микрошаг (1, 2, 4, 8 и т. д.)
#define BELT_PITCH          2  // шаг ремня (например, 2 мм)
#define NUMBER_TEETH_PULLEY 20  // количество зубьев на шкиве, на валу двигателя.

const std::string FILE_NAME = "CE3E3V2_xyzCalibration_cube.gcode";

class Matrix;

/** @brief Ошибка матрицы, с выводом сообщения
 * */
class MatrixException : public std::exception {
    std::string m_msg;
public:
    explicit MatrixException(const std::string &msg) : m_msg(msg) {}
    const char *what() const noexcept override { return m_msg.c_str(); }
};

/** @brief Ошибка матрицы при чтении из потока
 * */
class InvalidMatrixStream : public MatrixException {
public:
    InvalidMatrixStream() : MatrixException("Произошла ошибка при чтении из потока") {}
};

/** @brief Ошибка матрицы, выход за пределы матрицы
 * */
class OutOfRange : public MatrixException {
public:
    OutOfRange(size_t i, size_t j, const Matrix &matrix);
};


/* @class Matrix класс матрицы двумерной. Различные операции для расчетов
 * @param rows Строки
 * @param cols Колонки
 * @param matrix Массив элементов матрицы
 * */
class Matrix {
private:
    size_t rows;
    size_t cols;
    uint8_t* matrix;

    static inline uint8_t get_grey(const uint8_t& R, const uint8_t& G, const uint8_t& B) {
        return  uint8_t(float(R) * float(0.299) + float(G) * float(0.587) + float(B) * float(0.114));
    }
public:
    /** @defgroup Базовые операции
     *  В данной группе содержатся различные конструкторы, оператор присваивания, а так же методы
     *  получения/изменения элемента и получения размеров матрицы
     *  @{
     */

    /** @brief Конструктор выделяет под матрицу размерами rows x cols область памяти, заполненную 0-ями
     *  @param rows Колличество строк в матрице, считается по умолчанию, что он >= 0
     *  @param cols Колличество колонок в матрице, считается по умолчанию, что он >= 0
     * */
    explicit Matrix(const size_t& _rows = 0, const size_t& _cols = 0) : rows(_rows), cols(_cols) {
        matrix = new uint8_t[( rows * cols )];
        for ( size_t i = 0, size = ( rows * cols ); i < size; ++i )
		    matrix[i] = 0;
    }

    /** @brief Конструктор копирования
     *  @param rhs Другая матрица
     * */
    Matrix(const Matrix& rhs) noexcept {
        rows = rhs.getRows();
        cols = rhs.getCols();
	    matrix = new uint8_t[( rows * cols )];
        for ( size_t i = 0; i < rows; ++i )
		    for ( size_t j = 0; j < cols; ++j )
                matrix[( j + ( i * cols ) )] = rhs.matrix[( j + ( i * cols ) )];
    }

    /** @brief Оператор присваивания
     *  @param rhs Другая матрица
     *  @return Возвращает присвоенную матрицу
     * */
    Matrix &operator=(const Matrix& rhs) noexcept {
        if ( &rhs != this ) {
            delete[] matrix;
		    rows = rhs.getRows();
            cols = rhs.getCols();
            matrix = new uint8_t[( rows * cols )];
		    for ( size_t i = 0; i < rows; ++i )
                for ( size_t j = 0; j < cols; ++j )
                    matrix[( j + ( i * cols ) )] = rhs.matrix[( j + ( i * cols ) )];
	    }
        return *this;
    }

    /** @brief Деструктор, должен очистить память
     * */
    ~Matrix() noexcept {
        delete[] matrix;
    }

    /** @brief Метод получения колличества строк
     *  @return Возвращает переменную rows
     * */
    size_t getRows() const noexcept { return cols; }

    /** @brief Метод получения колличества колонок
     *  @return Возвращает переменную cols
     * */
    size_t getCols() const noexcept { return cols; }

	/** @brief Проверка на нулевую матрицу
	 *  @return Если матрица пустая вернет true, в противном случае false
	 * */
    bool isNull() const noexcept { return ((cols == 0) || (rows == 0)); }

    /** @brief Спомощью такой перегруженной функциональные формы происходит
     *         извлечение элемента без его изменения.
     *  @param i номер строки, если не входит в границы, вернуть 0
     *  @param j номер колонки, если не входит в границы, вернуть 0
     *  @return Возвращает элемент под номер строки i и колонки j
     *  @exception OutOfRange() Выход за пределы матрицы
     * */
    uint8_t operator()(size_t i, size_t j) const {
        if ( ( ( i >= rows ) || ( j >= cols ) ) )
            throw OutOfRange( i, j, *this );
	    return matrix[( j + ( i * cols ) )];
    }

    /** @brief Спомощью такой перегруженной функциональные формы происходит
     *         извлечение элемента для его дальнейшего изменения.
     *  @param i номер строки, если не входит в границы, вернуть 0
     *  @param j номер колонки, если не входит в границы, вернуть 0
     *  @return Возвращает элемент под номер строки i и колонки j, для его изменения
     *  @exception OutOfRange() Выход за пределы матрицы
     * */
    uint8_t& operator()(size_t i, size_t j) {
        if ( ( ( i >= rows ) || ( j >= cols ) ) )
            throw OutOfRange( i, j, *this );
	    return ( ( uint8_t& )matrix[( j + ( i * cols ) )] );
    }

    /** @} */ // Конец группы: Базовые операции

    /** @defgroup Математические операции над матрицами
     *  Булевые операции и арифметические операции(сложение, вычитание, умножение)
     *  @{
     */

    /** @brief Булевая операции равенство
     *  @param rhs Матрица
     *  @return true если матрицы равны, false если не равны
     * */
    bool operator==(const Matrix& rhs) const noexcept {
        if ( ( ( rows != rhs.rows ) || ( cols != rhs.cols ) ) )
            return false;
	    for ( size_t i = 0; i < rows; ++i )
            for ( size_t j = 0; j < cols; ++j )
			    if ( std::abs( matrix[( j + ( i * cols ) )] - rhs.matrix[( j + ( i * cols ) )]) > 1e-7 )
                    return false;
        return true;
    }

    /** @brief Булевая операци неравенства
     *  @param rhs Матрица
     *  @return false если матрицы равны, true если матрицы не равны
     * */
    bool operator!=(const Matrix& rhs) const noexcept { return !( *this == rhs ); }

    /** @} */ // Конец группы: Математические операции над матрицами

    /** @defgroup Дополнительные операции над матрицами
    *  @{
    */

    void openJpeg( const std::string& fileName ) {
        struct jpeg_decompress_struct d1;
        struct jpeg_error_mgr m1;
        d1.err = jpeg_std_error(&m1);
        jpeg_create_decompress(&d1);
        FILE *f = fopen(fileName.c_str(),"rb");
        jpeg_stdio_src(&d1, f);
        jpeg_read_header(&d1, TRUE);
        if( ( cols != 0 ) || ( rows != 0 ) )
            delete[] matrix;
        rows = d1.image_height;
        cols = d1.image_width;
        matrix = new uint8_t[( rows * cols )];

        jpeg_start_decompress(&d1);
        uint8_t *pBuf = new uint8_t[rows * cols * d1.num_components]{};
        for (size_t i = 0; d1.output_scanline < d1.output_height;) {
            i += jpeg_read_scanlines(&d1, (JSAMPARRAY)&(pBuf), 1);
            for (size_t j = 0; j < cols; ++j) {
                matrix[j + (i - 1) * cols] = get_grey(pBuf[j * d1.num_components + 2],
                                                      pBuf[j * d1.num_components + 1],
                                                      pBuf[j * d1.num_components + 0]);
            }
        }

        jpeg_finish_decompress(&d1);
        jpeg_destroy_decompress(&d1);
        fclose(f);
        delete[] pBuf;
    }

    void saveJpeg( const std::string& fileName ) {
        struct jpeg_compress_struct cinfo;  /* Шаг 1: выделите и инициализируйте объект сжатия JPEG */
        struct jpeg_error_mgr jerr;
        JSAMPROW row_pointer[1];
        cinfo.err = jpeg_std_error(&jerr);

        jpeg_create_compress(&cinfo);  /* Шаг 2: укажите место назначения данных  (eg, a file) */
        FILE *outfile = fopen( fileName.c_str(),"wb" );

        jpeg_stdio_dest(&cinfo, outfile); /* Шаг 3: установите параметры для сжатия */
        cinfo.image_width = cols;    /* Количество столбцов в изображении */
        cinfo.image_height = rows;   /* Количество строк в изображении */
        cinfo.input_components = 1;     // Каналы, RGB 3 ; GRAY 1
        cinfo.in_color_space = J_COLOR_SPACE(1);
        JSAMPLE* image_buffer = new JSAMPLE[cols * rows]();  /* Указывает на большой массив данных R, G, B-порядка */
        for (size_t i = 0; i < cols * rows; ++i) {
            image_buffer[i] = matrix[i];
        }
        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, 100, true);

        jpeg_start_compress(&cinfo, true);  /* Шаг 4: Запустите компрессор */

        while (cinfo.next_scanline < cinfo.image_height) {  /* Шаг 5: пока (строки сканирования еще предстоит записать)*/
            row_pointer[0] = (JSAMPROW)&image_buffer[cinfo.next_scanline * cols];
            (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_compress(&cinfo);  /* Шаг 6: Завершите сжатие */
        jpeg_destroy_compress(&cinfo);  /* Шаг 7: освободите объект сжатия JPEG */
        fclose(outfile);
    }

    void drawLine( int x0, int y0, int x1, int y1, const uint8_t& color ) {
        int A = ( y1 - y0 ), B = ( x0 - x1 );
        int sign = ( ( std::abs(A) > std::abs(B) ) ? 1 : -1 );
        int signa = ( ( A < 0 ) ? -1 : 1 ), signb = ( ( B < 0 ) ? -1 : 1 );
        int f = 0;
        matrix[(  x0 + ( y0 * cols )  )] = color;
        int x = x0, y = y0;
        if( sign == -1 ) {
            do {
                f += ( A * signa );
                if( f > 0 ) {
                    f -= ( B * signb );
                    y += signa;
                }
                x -= signb;
                matrix[( x + ( y * cols ) )] = color;
            } while (x != x1 || y != y1);
        } else {
            do {
                f += B * signb;
                if( f > 0 ) {
                    f -= A * signa;
                    x -= signb;
                }
                y += signa;
                matrix[( x + ( y * cols ) )] = color;
            } while( ( x != x1 ) || ( y != y1 ) );
        }
    }

    void clear() {
        for( size_t i = 0; i < ( rows * cols ); ++i )
            matrix[i] = 0;
    }

    /** @} */ // Конец группы: Дополнительные операции над матрицами
};

OutOfRange::OutOfRange(size_t i, size_t j, const Matrix &matrix) : MatrixException(
    "Индексы (" + std::to_string(i) + ", " + std::to_string(j) +
    ") выход за границы матрицы. Размер матрицы [" +
    std::to_string(matrix.getRows()) + ", " + std::to_string(matrix.getCols()) + "]"
) {}

class CNCException : public std::exception {
    std::string m_msg;
public:
    explicit CNCException(const std::string &msg) : m_msg(msg) {}
    const char *what() const noexcept override { return m_msg.c_str(); }
};

class UnknownGCode : public CNCException {
    std::string m_msg;
public:
    explicit UnknownGCode( const std::string &code ) : CNCException( "Неизвестный код!: " + code ) {}
};

class FileNotOpen : public std::exception {
    std::string m_msg;
public:
    explicit FileNotOpen(const std::string &msg) : m_msg(msg) {}
    const char *what() const noexcept override { return m_msg.c_str(); }
};

struct Axes {
    float _x;
    float _y;
    float _z;
    float _e;
    uint16_t _f;
    Axes() : _x(0), _y(0), _z(0), _e(0), _f(0.0) {}
    Axes( const float& x, const float& y, const float& z, const float& e, const uint16_t& f ) : _x(x), _y(y),
        _z(z), _e(e), _f(f) {}
};

class StepperMotor {
public:
    virtual ~StepperMotor() {}

    virtual void moveE( const Axes& ax ) = 0;
    virtual void move( const Axes& ax ) = 0;
    virtual void setting( const Axes& ax ) = 0;

    virtual void on() = 0;
    virtual void off() = 0;

    virtual void relativeAxes() = 0;
    virtual void absoluteAxes() = 0;
};

#define MATRIX_SCALER_SIZE 1000
#define TABLE_SIZE 2.2

class MatrixMotor : public StepperMotor {
    int _prevX = 0, _prevY = 0, _prevZ = 0, _prevE = 0;
    int _x = 0, _y = 0, _z = 0, _e = 0;
    Matrix m;

    bool isWork;
    void saveLayer( const float& layer ) {
        static int i = 0;
        std::string strL = std::to_string( ( std::round( layer * 10 ) / 10 ) );
        strL = strL.substr( 0, ( strL.length() - 5 ) );
        m.saveJpeg( ( "img/layer_" + std::to_string( i++ ) + "_" + strL + ".jpg" ) );
        m.clear();
    }

public: 
    MatrixMotor() {
        Matrix mat( ( TABLE_SIZE * MATRIX_SCALER_SIZE ), ( TABLE_SIZE * MATRIX_SCALER_SIZE ) );
        m = mat;
        isWork = true;
    }

    ~MatrixMotor() {}

    void moveE( const Axes& ax ) override {
        if( !isWork )
            return;

        _x = ( ( ax._x != 0 ) ? std::round( ax._x * 10 ) : _prevX );
        _y = ( ( ax._y != 0 ) ? std::round( ax._y * 10 ) : _prevY );
        if( ax._z != 0 )
            saveLayer( ax._z );

        if( ( ( _prevX == _x ) && ( _prevY == _y ) ) )
            return;

        m.drawLine( _prevX, _prevY, _x, _y, 255 );
        setting( ax );
    }

    void move( const Axes& ax ) override {
        if( !isWork )
            return;

        _x = ( ( ax._x != 0 ) ? std::round( ax._x * 10 ) : _prevX );
        _y = ( ( ax._y != 0 ) ? std::round( ax._y * 10 ) : _prevY );
        if( ax._z != 0 )
            saveLayer( ax._z );
        
        if( ( ( _prevX == _x ) && ( _prevY == _y ) ) )
            return;
        setting( ax );
    }

    void setting( const Axes& ax ) override {
        _prevX = ( ( ax._x != 0 ) ? std::round( ax._x * 10 ) : _prevX );
        _prevY = ( ( ax._y != 0 ) ? std::round( ax._y * 10 ) : _prevX );
        _prevZ = ( ( ax._z != 0 ) ? std::round( ax._z * 10 ) : _prevX );
        _prevE = ( ( ax._e != 0 ) ? std::round( ax._e * 10 ) : _prevX );
    }

    void on() override {
        isWork = true;
        std::cout << "---> Моторы включены" << std::endl;
    }

    void off() override {
        isWork = false;
        std::cout << "---> Моторы отключены" << std::endl;
    }

    void relativeAxes() override {
        std::cout << "---> Установлены относительные координаты" << std::endl;
    }
    
    void absoluteAxes() override {
        std::cout << "---> Установлены абсолютные координаты" << std::endl;
    }
};

class Arbitr {
private:
    using cfp = std::pair<char, float>;
    std::ifstream inFile;
    size_t fileSize;
    size_t currentSize;
    StepperMotor* motors;

    std::vector<std::string> split( const std::string &s, char delim ) {
        std::vector<std::string> elems;
        std::stringstream ss(s);
        std::string item;
        while( std::getline( ss, item, delim ) )
            elems.push_back(item);
        return elems;
    }

    cfp getCmdId( std::string id ) {
        char code = id[0];
        id.erase(0, 1);
        return cfp( code, ( ( !id.empty() ) ? std::stof(id) : std::numeric_limits<float>::min() ) );
    }

    Axes getAxes( const cfp* pairs, const size_t& size ) {
        Axes ax;
        for( size_t i = 0; i < size; ++i ) {
            switch( pairs[i].first ) {
                case 'X':
                    ax._x = pairs[i].second;
                    break;
                case 'Y':
                    ax._y = pairs[i].second;
                    break;
                case 'Z':
                    ax._z = pairs[i].second;
                    break;
                case 'E':
                    ax._e = pairs[i].second;
                    break;
                case 'F':
                    ax._f = pairs[i].second;
                    break;
                default:
                    break;
            }
        }
        return ax;
    }

    void G0( const cfp* pairs, const size_t& size ) {
        Axes ax = getAxes( pairs, size );
        motors->move( ax );
    }

    void G1( const cfp* pairs, const size_t& size ) {
        Axes ax = getAxes( pairs, size );
        motors->moveE( ax );
    }

    void G28() {
        std::cout << "G28: Перейти в точку 0" << std::endl;
        motors->move( Axes() );
    }

    void G90() {
        std::cout << "G90: Установка абсолютных координат" << std::endl;
        motors->absoluteAxes();
    }

    void G91() {
        std::cout << "G91: Установка относительных координат" << std::endl;
        motors->relativeAxes();
    }

    void G92() {
        std::cout << "G92: сброс всех значений" << std::endl;
        motors->setting( Axes() );
    }

    void M82() {
        std::cout << "M82: Установить экструдер в абсолютный режим" << std::endl;
    }

    void M84() {
        std::cout << "M84: Отключить моторы" << std::endl;
        motors->off();
    }

    void M104( const cfp* pairs, const size_t& size ) {
        std::cout << "M104: Установить температуру экструдера на "
                  << std::to_string(((int)pairs[0].second))
                  << " Градусов. Не ждать установки" << std::endl;
    }

    void M105( const cfp* pairs, const size_t& size ) {
        std::cout << "M105: Получить данные о температуре экструдера и стола" << std::endl;
    }

    void M106( const cfp* pairs, const size_t& size ) {
        std::cout << "M106: Включить вентилятор охлаждения модели. Мощность: "
                  << std::round( pairs[0].second / 255 * 100 ) << " %" << std::endl;
    }

    void M107() {
        std::cout << "M107: Выключить вентилятор охлаждения модели" << std::endl;
    }

    void M109( const cfp* pairs, const size_t& size ) {
        std::cout << "M109: Установить температуру экструдера на "
                  << std::to_string(((int)pairs[0].second))
                  << " Градусов. Ждать установки" << std::endl;
    }

    void M140( const cfp* pairs, const size_t& size ) {
        std::cout << "M140: Установить температуру стола на "
                  << std::to_string(((int)pairs[0].second))
                  << " Градусов. Не ждать установки" << std::endl;
    }

    void M190( const cfp* pairs, const size_t& size ) {
        std::cout << "M190: Установить температуру стола на "
                  << std::to_string(((int)pairs[0].second))
                  << " Градусов. Ждать установки" << std::endl;
    }

    void callCode( const std::string& cmd, const cfp* pairs, const size_t& size ) {
        if( cmd == "G0" )
            G0( pairs, size );
        else if( cmd == "G1" )
            G1( pairs, size );
        else if( cmd == "G28" )
            G28();
        else if( cmd == "G90" )
            G90();
        else if( cmd == "G91" )
            G91();
        else if( cmd == "G92" )
            G92();
        else if( cmd == "M82" )
            M82();
        else if( cmd == "M84" )
            M84();
        else if( cmd == "M104" )
            M104( pairs, size );
        else if( cmd == "M105" )
            M105( pairs, size );
        else if( cmd == "M106" )
            M106( pairs, size );
        else if( cmd == "M107" )
            M107();
        else if( cmd == "M109" )
            M109( pairs, size );
        else if( cmd == "M140" )
            M140( pairs, size );
        else if( cmd == "M190" )
            M190( pairs, size );
        else
            throw UnknownGCode(cmd);
    }

public:
    
    Arbitr( const std::string& fileName, StepperMotor* m ) : inFile(fileName), fileSize(0), currentSize(0) {
        if( !inFile )
            throw FileNotOpen( fileName );
        inFile.seekg( 0, std::ios::end );
        fileSize = inFile.tellg();
        inFile.seekg(0);
        motors = m;
    }
    
    ~Arbitr() {
        motors->off();
        inFile.close();
    }

    int make() {
        for( std::string strReaded = "", strClear = ""; inFile; std::getline( inFile, strReaded ) ) {
            currentSize += ( strReaded.size() + 1 );
            strClear = strReaded.substr( 0, strReaded.find(";") );
            if( strClear.empty() )
                continue;
            auto commands = split( strClear, ' ' );
            const size_t size = ( commands.size() - 1 );
            cfp* pairs = new cfp[size];
            const std::string cmd = commands[0];
            for( size_t i = 1; i < commands.size(); ++i )
                pairs[( i - 1 )] = getCmdId(commands[i]);
            try {
                callCode( cmd, pairs, size );
            } catch ( const UnknownGCode& ugc ) {
                delete[] pairs;
                std::cout << ( std::round( float(currentSize) / float(fileSize) * 100.0 ) / 100.0 ) << " %" << std::endl;
                std::cout << ugc.what() << std::endl;
                return -1;
            };
            delete[] pairs;
        }
        return 0;
    }
};

int main() {
    struct stat st;
    if( !( ( stat( "img", &st ) == 0 ) && S_ISDIR(st.st_mode) ) )
        if ( mkdir( "img", ( S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) ) != 0 )
            throw;
    MatrixMotor mm;
    Arbitr arbitr( FILE_NAME, &mm );
    return arbitr.make();
}


