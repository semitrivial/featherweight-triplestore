import java.io.File;
import java.util.Set;
import org.semanticweb.owlapi.apibinding.OWLManager;
import org.semanticweb.owlapi.model.OWLOntologyManager;
import org.semanticweb.owlapi.model.OWLOntology;
import org.semanticweb.owlapi.model.OWLOntologyLoaderConfiguration;
import org.semanticweb.owlapi.model.*;
import org.semanticweb.owlapi.reasoner.*;
import org.semanticweb.owlapi.io.OWLOntologyDocumentSource;
import org.semanticweb.owlapi.io.FileDocumentSource;
import org.coode.owlapi.manchesterowlsyntax.ManchesterOWLSyntaxEditorParser;
import org.semanticweb.owlapi.util.BidirectionalShortFormProvider;
import org.semanticweb.owlapi.util.BidirectionalShortFormProviderAdapter;
import org.semanticweb.owlapi.util.SimpleShortFormProvider;
import org.semanticweb.owlapi.util.AnnotationValueShortFormProvider;
import org.semanticweb.owlapi.util.OWLOntologyImportsClosureSetProvider;
import org.semanticweb.owlapi.expression.OWLEntityChecker;
import org.semanticweb.owlapi.expression.ShortFormEntityChecker;
import org.semanticweb.elk.owlapi.ElkReasonerFactory;

public class Convert
{
  public static void main(String [] args) throws Exception
  {
    Convert c = new Convert();
    c.run(args);
  }

  public void run(String [] args) throws Exception
  {
    OWLOntologyManager manager = OWLManager.createOWLOntologyManager();
    OWLOntologyLoaderConfiguration config = new OWLOntologyLoaderConfiguration();
    config.setMissingOntologyHeaderStrategy(OWLOntologyLoaderConfiguration.MissingOntologyHeaderStrategy.IMPORT_GRAPH);

    File kbfile;
    OWLOntology ont;

    if ( args.length != 1 )
    {
      System.err.println( "Syntax: java Convert (owlfile) >(outputfile)" );
      System.err.println( "..." );
      System.err.println( "For example: java Convert /home/ricordo/ontology/ricordo.owl >ricordo.feather" );
      return;
    }

    try
    {
      kbfile = new File(args[0]);
      ont = manager.loadOntologyFromOntologyDocument(new FileDocumentSource(kbfile),config);
    }
    catch(Exception e)
    {
      System.out.println("Load failure");
      return;
    }

    IRI iri = manager.getOntologyDocumentIRI(ont);

    OWLDataFactory df = OWLManager.getOWLDataFactory();
    Set<OWLOntology> onts = ont.getImportsClosure();

    OWLReasonerFactory rf = new ElkReasonerFactory();
    OWLReasoner r = rf.createReasoner(ont);

    for ( OWLOntology o : onts )
    {
      Set<OWLClass> classes = o.getClassesInSignature();

      for ( OWLClass c : classes )
      {
        NodeSet<OWLClass> subClasses = r.getSubClasses(c, true);

        for (Node<OWLClass> subnode : subClasses )
        {
          OWLClass sub = subnode.getEntities().iterator().next();

          if ( sub.isOWLNothing() )
            continue;

          System.out.print( "Sub " + c.toStringID() + " " + sub.toStringID() + "\n" );
        }

        Set<OWLClassExpression> supers = c.getSuperClasses(o);
        for (OWLClassExpression exp: supers)
        {
          if ( !(exp instanceof OWLObjectSomeValuesFrom) )
            continue;

          OWLRestriction restrict = (OWLRestriction) exp;

          Set<OWLObjectProperty> objprops = restrict.getObjectPropertiesInSignature();
          boolean fBad = false;

          for (OWLObjectProperty objprop : objprops )
          {
            if ( !objprop.toStringID().equals( "http://purl.org/obo/owlapi/fma#regional_part_of" )
            &&   !objprop.toStringID().equals( "http://purl.org/obo/owlapi/fma#constitutional_part_of" ) )
            {
              fBad = true;
              break;
            }
          }
          if ( fBad )
            continue;

          System.out.print( "Part " );
          Set<OWLClass> classes_in_signature = restrict.getClassesInSignature();
          for (OWLClass class_in_signature : classes_in_signature)
          {
            System.out.print( class_in_signature.toStringID() );
            break;
          }
          System.out.print( " " + c.toStringID() + "\n" );
        }
      }
    }

    return;
  }
}

