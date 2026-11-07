from pathlib import Path
import re, html, json, argparse
from reportlab.graphics import renderSVG
from reportlab.pdfgen import canvas
from reportlab.platypus import BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, PageBreak, Image, Table, TableStyle, KeepTogether, CondPageBreak, Flowable
from reportlab.platypus.tableofcontents import TableOfContents
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from PIL import Image as PILImage
from guide_figure6 import validation_diagram

documentation=Path(__file__).resolve().parents[1]
root=documentation/'docs'
metadata=json.loads((documentation/'user-guide.json').read_text(encoding='utf-8'))
version=metadata['version'];status=metadata['status'];review_date=metadata['review_date']
is_draft=status!='RELEASE'
version_label=f'Version {version}'+(' - DRAFT' if is_draft else '')
applicability=f'Applicable to vFPC {version}'+(' pre-release' if is_draft else '')
parser=argparse.ArgumentParser(description='Build the User Guide PDF from its shared Markdown sources.')
parser.add_argument('--output',type=Path,default=root/'downloads'/metadata.get('pdf_filename',f'vFPC-User-Guide-v{version}-DRAFT.pdf'))
args=parser.parse_args();output=args.output.resolve();output.parent.mkdir(parents=True,exist_ok=True)
for name,file in [('Guide','arial.ttf'),('Guide-Bold','arialbd.ttf'),('Guide-Italic','ariali.ttf'),('Guide-BoldItalic','arialbi.ttf')]:
 pdfmetrics.registerFont(TTFont(name,'C:/Windows/Fonts/'+file))
pdfmetrics.registerFontFamily('Guide',normal='Guide',bold='Guide-Bold',italic='Guide-Italic',boldItalic='Guide-BoldItalic')
styles=getSampleStyleSheet()
styles.add(ParagraphStyle(name='BodyGuide',fontName='Guide',fontSize=10.5,leading=13,spaceAfter=5))
styles.add(ParagraphStyle(name='H1Guide',parent=styles['BodyGuide'],fontName='Guide-Bold',fontSize=22,leading=27,spaceAfter=17,keepWithNext=True))
styles.add(ParagraphStyle(name='H2Guide',parent=styles['BodyGuide'],fontName='Guide-Bold',fontSize=15,leading=19,spaceBefore=12,spaceAfter=8,keepWithNext=True))
styles.add(ParagraphStyle(name='H3Guide',parent=styles['BodyGuide'],fontName='Guide-Bold',fontSize=12,leading=16,spaceBefore=9,spaceAfter=6,keepWithNext=True))
styles.add(ParagraphStyle(name='ExampleGuide',parent=styles['BodyGuide'],leftIndent=12,rightIndent=10,backColor=colors.HexColor('#F0F4F7'),borderPadding=7,spaceBefore=4,spaceAfter=13))
styles.add(ParagraphStyle(name='NoteGuide',parent=styles['BodyGuide'],fontSize=10,leading=13,leftIndent=10,rightIndent=8,spaceBefore=5,spaceAfter=11))
styles.add(ParagraphStyle(name='CaptionGuide',parent=styles['BodyGuide'],fontName='Guide-Italic',fontSize=9,leading=12,textColor=colors.HexColor('#555555'),spaceAfter=10))
styles.add(ParagraphStyle(name='CellGuide',parent=styles['BodyGuide'],fontSize=9.5,leading=12,spaceAfter=0))
styles.add(ParagraphStyle(name='TitleGuide',fontName='Guide-Bold',fontSize=33,leading=39,alignment=TA_CENTER,spaceAfter=17))
styles.add(ParagraphStyle(name='CoverGuide',parent=styles['BodyGuide'],fontSize=14,leading=20,alignment=TA_CENTER,spaceAfter=12))
styles.add(ParagraphStyle(name='BulletGuide',parent=styles['BodyGuide'],leftIndent=15,firstLineIndent=0,bulletIndent=1,spaceAfter=3))
styles.add(ParagraphStyle(name='SupportBody',parent=styles['BodyGuide'],leading=12,spaceAfter=4))
styles.add(ParagraphStyle(name='SupportBullet',parent=styles['BulletGuide'],leading=12,spaceAfter=2))
# Match the released PDF typography, palette and hierarchy.
slate=colors.HexColor('#233746');teal=colors.HexColor('#176B78');muted=colors.HexColor('#566571')
for name in ['BodyGuide','BulletGuide','NoteGuide','CellGuide','SupportBody','SupportBullet']:
 styles[name].textColor=slate
for name in ['BodyGuide','BulletGuide','NoteGuide','CellGuide','SupportBody','SupportBullet','ExampleGuide']:
 styles[name].allowWidows=0;styles[name].allowOrphans=0
styles['BodyGuide'].fontSize=9.5;styles['BodyGuide'].leading=12.5;styles['BodyGuide'].spaceAfter=6
for name in ['BulletGuide','SupportBullet']:
 styles[name].fontSize=9.5;styles[name].leading=12;styles[name].spaceAfter=3
styles['H1Guide'].fontSize=24;styles['H1Guide'].leading=29;styles['H1Guide'].textColor=slate;styles['H1Guide'].spaceAfter=15
styles['H2Guide'].fontSize=14;styles['H2Guide'].leading=18;styles['H2Guide'].textColor=teal
styles['H3Guide'].fontSize=11;styles['H3Guide'].leading=15;styles['H3Guide'].textColor=slate
styles['NoteGuide'].fontSize=9.5;styles['NoteGuide'].leading=12.5;styles['NoteGuide'].leftIndent=0;styles['NoteGuide'].rightIndent=0;styles['NoteGuide'].spaceAfter=0;styles['NoteGuide'].spaceBefore=0
styles['CaptionGuide'].fontSize=8.5;styles['CaptionGuide'].leading=11;styles['CaptionGuide'].textColor=muted;styles['CaptionGuide'].alignment=TA_CENTER
styles['CellGuide'].fontSize=9;styles['CellGuide'].leading=11.5
styles['ExampleGuide'].fontSize=9.5;styles['ExampleGuide'].leading=12.5;styles['ExampleGuide'].textColor=slate;styles['ExampleGuide'].backColor=colors.HexColor('#F1F5F7')
styles['SupportBody'].fontSize=9.5;styles['SupportBody'].leading=12;styles['SupportBody'].spaceAfter=5
styles['TitleGuide'].fontSize=34;styles['TitleGuide'].leading=41;styles['TitleGuide'].alignment=0;styles['TitleGuide'].textColor=slate
styles['CoverGuide'].fontSize=9.5;styles['CoverGuide'].leading=15;styles['CoverGuide'].alignment=0;styles['CoverGuide'].textColor=slate

class NotePanel(Flowable):
 def __init__(self,paragraph):
  Flowable.__init__(self);self.paragraph=paragraph;self.spaceBefore=5;self.spaceAfter=11
 def wrap(self,aW,aH):
  self.width=aW;_,ph=self.paragraph.wrap(aW-24,aH);self.ph=ph;self.height=ph+16;return self.width,self.height
 def draw(self):
  c=self.canv;c.setFillColor(colors.HexColor('#F0F4F6'));c.rect(0,0,self.width,self.height,fill=1,stroke=0)
  c.setStrokeColor(teal);c.setLineWidth(1.6);c.line(0,0,0,self.height)
  self.paragraph.drawOn(c,11,8)

W,H=A4
slug=lambda s:re.sub('[^a-z0-9 -]','',s.lower()).replace(' ','-')
sections=metadata['chapters']
def linktarget(url):
 if re.match(r'https?://',url):return url
 file,_,anchor=url.partition('#')
 if file in sections or file=='index.md':return '#'+(file.removesuffix('.md')+'-'+anchor if anchor else file.removesuffix('.md'))
 return None
def inline(text):
 text=re.sub(r'\s*\{[^{}]*\}', '',text)
 text=text.replace('🔗 ','').replace('\u00a0',' ')
 # Existing guide spans are UI labels and colour swatches.
 mapping={'green':'#087F23','yellow':'#8A6600','red':'#B51818','grey':'#666666','cyan':'#007E86','amber':'#956000'}
 def span(m):
  val=re.search(r'--(green|yellow|red|grey|cyan|amber)',m[1]);color=mapping[val[1]] if val else '#000000'
  return '<font color="'+color+'"><b>'+m[2]+'</b></font>'
 text=re.sub(r'<span\s+class="([^"]*)">(.*?)</span>',span,text)
 # Escape prose while retaining only tags intentionally generated here.
 tokens=[]
 def save(m):tokens.append(m[0]);return f'ZZTAG{len(tokens)-1}ZZ'
 text=re.sub(r'</?(?:font|b)[^>]*>',save,text)
 text=html.escape(text,quote=False)
 text=re.sub(r'`([^`]+)`',lambda m:'<font name="Courier" size="9">'+m[1]+'</font>',text)
 text=re.sub(r'\*\*(.+?)\*\*',r'<b>\1</b>',text)
 text=re.sub(r'(?<!\*)\*([^*]+)\*(?!\*)',r'<i>\1</i>',text)
 def link(m):
  target=linktarget(html.unescape(m[2]));return '<link href="'+html.escape(target,quote=True)+'" color="#165B85">'+m[1]+'</link>' if target else m[1]
 text=re.sub(r'\[([^]]+)\]\(([^)]+)\)',link,text)
 for i,tag in enumerate(tokens):text=text.replace(f'ZZTAG{i}ZZ',tag)
 return text

class GuideDoc(BaseDocTemplate):
 def afterFlowable(self,flowable):
  if hasattr(flowable,'bookmark'):
   self.canv.bookmarkPage(flowable.bookmark)
   if flowable.style.name=='H1Guide':
    self.canv.addOutlineEntry(flowable.getPlainText(),flowable.bookmark,0,False)
    self.notify('TOCEntry',(0,flowable.getPlainText(),self.page,flowable.bookmark))

def page(c,doc):
 c.saveState()
 if doc.page>1:
  c.setFillColor(muted);c.setFont('Guide',8)
  c.drawString(50,H-27,'vFPC User Guide')
  c.drawRightString(W-50,H-27,version_label)
  c.setStrokeColor(colors.HexColor('#D8E0E4'));c.setLineWidth(.5);c.line(50,H-36,W-50,H-36)
  c.drawString(50,29,applicability)
  c.drawRightString(W-50,29,str(doc.page))
 c.restoreState()

doc=GuideDoc(str(output),pagesize=A4,leftMargin=55,rightMargin=55,topMargin=57,bottomMargin=47,title=f'vFPC User Guide {version}'+(' Draft' if is_draft else ''),author='vFPC',allowSplitting=1)
frame=Frame(55,47,W-110,H-104,leftPadding=0,rightPadding=0,topPadding=0,bottomPadding=0)
doc.addPageTemplates(PageTemplate(id='Guide',frames=frame,onPage=page))
story=[Spacer(1,80)]
logo=root/'images/vfpc-guide-cover.png'
story.extend([Image(str(logo),width=453.54,height=181.37),Spacer(1,48),Paragraph('vFPC User Guide',styles['TitleGuide']),Paragraph(f'<font size="21" color="#176B78">{version_label}</font>',styles['CoverGuide']),Spacer(1,20),Paragraph('VATSIM Flight Plan Checker for EuroScope',styles['CoverGuide']),Paragraph(applicability,styles['CoverGuide']),Paragraph(f'Draft for review - {review_date}' if is_draft else '',styles['CoverGuide']),PageBreak()])

def parse_markdown(filename):
 source=(root/filename).read_text(encoding='utf-8-sig')
 if filename=='index.md':source=re.sub(r'^## PDF User Guide\n.*?(?=^## |\Z)','',source,flags=re.M|re.S)
 lines=source.splitlines();i=0;items=[]
 while i<len(lines):
  raw=lines[i];s=raw.strip()
  if not s:i+=1;continue
  if s=='---':items.append(Spacer(1,6));i+=1;continue
  if s.startswith('!!!'):
   title=re.search(r'"([^"]+)"',s);label=title[1] if title else s[3:].strip().title()
   body=[];i+=1
   while i<len(lines) and (not lines[i].strip() or lines[i].startswith((' ','\t'))):
    if lines[i].strip():body.append(lines[i].strip())
    i+=1
   items.append(NotePanel(Paragraph('<b>'+inline(label)+'</b><br/>'+inline(' '.join(body)),styles['NoteGuide'])));continue
  if s.startswith('#'):
   m=re.match(r'^(#{1,3}) (.*)',s)
   if m:
    level=len(m[1]);para=Paragraph(inline(m[2]),styles[f'H{level}Guide']);para.bookmark=filename.removesuffix('.md')+('-'+slug(m[2]) if level>1 else '')
    items.append(para);i+=1;continue
  image=re.search(r'!\[([^]]*)\]\(([^)]+)\)',s)
  if image:
   path=root/image[2]
   if 'validation-flow' in str(path):
    items.append(KeepTogether([validation_diagram(),Spacer(1,5),Paragraph('Figure 6: Simplified vFPC validation process.',styles['CaptionGuide'])]))
    i+=1
    while i<len(lines) and not lines[i].strip():i+=1
    if i<len(lines) and lines[i].strip().startswith('*Figure'):i+=1
    continue
   im=PILImage.open(path)
   width=min(W-110,im.width*.65)
   if '50percent' in s:width=min(width,(W-110)*.50)
   height=width*im.height/im.width
   maxheight=460 if 'validation-flow' in str(path) else 240
   if height>maxheight:width*=maxheight/height;height=maxheight
   picture=Image(str(path),width=width,height=height);picture.hAlign='CENTER'
   group=[Spacer(1,5),picture,Spacer(1,5)]
   j=i+1
   while j<len(lines) and not lines[j].strip():j+=1
   if j<len(lines) and lines[j].strip().startswith('*Figure'):
    group.append(Paragraph(inline(lines[j].strip()),styles['CaptionGuide']));i=j
   items.append(KeepTogether(group));i+=1;continue
  if s.startswith('|'):
   rows=[]
   while i<len(lines) and lines[i].strip().startswith('|'):
    cells=[x.strip() for x in lines[i].strip().strip('|').split('|')]
    if not all(re.fullmatch(r'[: -]+',x) for x in cells):rows.append(cells)
    i+=1
   widths=[(W-110)*.27,(W-110)*.73] if len(rows[0])==2 else [(W-110)/len(rows[0])]*len(rows[0])
   table=Table([[Paragraph(('<b>'+inline(x)+'</b>') if r==0 else inline(x),styles['CellGuide']) for x in row] for r,row in enumerate(rows)],colWidths=widths,repeatRows=1,hAlign='LEFT')
   commands=[('BACKGROUND',(0,0),(-1,0),colors.HexColor('#E6F0F2')),('LINEBELOW',(0,0),(-1,0),.7,teal),('VALIGN',(0,0),(-1,-1),'MIDDLE'),('LEFTPADDING',(0,0),(-1,-1),8),('RIGHTPADDING',(0,0),(-1,-1),8),('TOPPADDING',(0,0),(-1,-1),5),('BOTTOMPADDING',(0,0),(-1,-1),5)]
   for r in range(1,len(rows)):
    if r%2:commands.append(('BACKGROUND',(0,r),(-1,r),colors.HexColor('#F4F6F8')))
   table.setStyle(TableStyle(commands));items.extend([table,Spacer(1,10)]);continue
  quote=s.startswith('>');bullet=re.match(r'^(?:([-*])|(\d+)\.)\s+(.*)',s)
  text=s[1:].strip() if quote else (bullet[3] if bullet else s)
  i+=1
  while i<len(lines) and lines[i].strip():
   nxt=lines[i].strip()
   if nxt.startswith(('#','!!!','|','>','![')) or re.match(r'^(?:[-*]|\d+\.)\s',nxt) or nxt=='---':break
   text+=' '+nxt;i+=1
  if quote:style=styles['ExampleGuide']
  elif bullet:style=styles['BulletGuide']
  else:style=styles['BodyGuide']
  if filename=='reporting-issues.md' and not quote:style=styles['SupportBullet' if bullet else 'SupportBody']
  para=Paragraph(inline(text),style,bulletText=('-' if bullet and bullet[1] else bullet[2]+'.') if bullet else None)
  if not bullet and not quote and text.endswith(':'):para.keepWithNext=True
  items.append(para)
 # Short menu/action lists should stay together instead of splitting a few items.
 grouped=[];j=0
 while j<len(items):
  if isinstance(items[j],Paragraph) and items[j].style.name=='BulletGuide':
   k=j
   while k<len(items) and isinstance(items[k],Paragraph) and items[k].style.name=='BulletGuide':k+=1
   grouped.append(KeepTogether(items[j:k])) if k-j<=8 else grouped.extend(items[j:k]);j=k
  else:grouped.append(items[j]);j+=1
 return grouped

for index,filename in enumerate(sections):
 if index:story.append(PageBreak())

 story.extend(parse_markdown(filename))
svg=renderSVG.drawToString(validation_diagram()).replace('font-family: Guide-Bold','font-family: Arial; font-weight: bold').replace('font-family: Guide','font-family: Arial')
(root/'images/validation-flow.svg').write_text(svg,encoding='utf-8')
doc.multiBuild(story)
print(output.resolve())
