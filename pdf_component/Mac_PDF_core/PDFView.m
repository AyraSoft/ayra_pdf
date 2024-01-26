/*
     ______  __    __  ______   ______
    |      \|  \  |  \/      \ |      \
     \▓▓▓▓▓▓\ ▓▓  | ▓▓  ▓▓▓▓▓▓\ \▓▓▓▓▓▓\
     /      ▓▓ ▓▓  | ▓▓ ▓▓   \▓▓/      ▓▓   Copyright 2023
    |  ▓▓▓▓▓▓▓ ▓▓__/ ▓▓ ▓▓     |  ▓▓▓▓▓▓▓   Ayra Soft
     \▓▓    ▓▓\▓▓    ▓▓ ▓▓      \▓▓    ▓▓   www.ayra.live
      \▓▓▓▓▓▓▓_\▓▓▓▓▓▓▓\▓▓       \▓▓▓▓▓▓▓
             |  \__| ▓▓
              \▓▓    ▓▓
                \▓▓▓▓▓▓
    
 Ayra uses a GPL/commercial licence - see LICENCE.md for details.
*/

@interface PDFView : NSView

@property (nonatomic, assign) CGPDFDocumentRef pdfDocument;
@property (nonatomic, assign) NSInteger currentPage;
@property (nonatomic, assign) CGFloat zoomLevel;
@property (nonatomic, assign) CGPoint topLeftOrigin;


- (instancetype)initWithFrame:(NSRect)frameRect;
- (void)dealloc;

@end

@implementation PDFView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    
    if (self) 
    {
        [self checkCurrentpage];
        
        self.zoomLevel = 1;
        self.topLeftOrigin = CGPointMake(0.0, 0.0);
        self.clipsToBounds = YES;
    }
    
    return self;
}

- (void)dealloc
{
    // Distruttore
    if (self.pdfDocument != NULL)
    {
        CGPDFDocumentRelease(self.pdfDocument);
        self.pdfDocument = NULL;
    }
    
    [super dealloc];
}

- (void) checkCurrentpage
{
    if (self.pdfDocument == NULL)
    {
        self.currentPage = -1;
    }
}

- (CGRect)getCurrentPageBounds
{
    CGPDFPageRef pdfPage = CGPDFDocumentGetPage(self.pdfDocument, self.currentPage);
    return CGPDFPageGetBoxRect(pdfPage, kCGPDFMediaBox);
}

//==============================================================================

- (void)drawRect:(NSRect)dirtyRect
{
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];

    if (self.pdfDocument != NULL)
    {
        CGPDFPageRef pdfPage = CGPDFDocumentGetPage(self.pdfDocument, self.currentPage);

        CGContextSaveGState(context);
        
/* NB: qui le prossime due linee colorano tutto il background, ma quando si istanzia una NSViewComponent
    questa si prende tutta l'area della window in cui e', quindi il background bianco lo coloro nella classe
    MacPDFViewComponent che colora solo la sua area
 */
        
//        CGContextSetRGBFillColor(context, 1.0, 1.0, 1.0, 1.0);
//        CGContextFillRect(context, CGContextGetClipBoundingBox(context));
        
        CGAffineTransform translation = CGAffineTransformMakeTranslation(self.topLeftOrigin.x, self.topLeftOrigin.y);
        CGAffineTransform pdfTransform = CGPDFPageGetDrawingTransform(pdfPage, kCGPDFMediaBox, self.bounds, 0, true);
        
        pdfTransform = CGAffineTransformScale(pdfTransform, self.zoomLevel, self.zoomLevel);
        pdfTransform = CGAffineTransformConcat(pdfTransform, translation);
        
        CGContextConcatCTM(context, pdfTransform);

        CGContextDrawPDFPage(context, pdfPage);

        CGContextRestoreGState(context);
    }
}

@end
